#include <stddef.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/crc16.h>

#include <g4config.h>
#include "config.h"
#include <mpc.h>
#include <util.h>

#include "comm.h"
#include "mpctwi.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#define mpc_crc(crc, data) ((MPC_DISABLE_CRC) ? 0 : _crc_ibutton_update(crc, data))

static void mpc_rx_event(comm_driver_t *);

#ifdef MPC_TWI
static comm_driver_t *comm;
#endif

#ifdef PHASOR_COMM_USART
#define PHASOR_COMM 1
#include "uart.h"
#include "serialcomm.h"
static serialcomm_t *phasor_comm;
static uart_dev_t *phasor_uart_dev;
#else
#endif

static void handle_master_hello(const mpc_pkt *const);
//static void handle_master_settings(const mpc_pkt *const);

static mempool_t *mempool;
static QueueHandle_t mpc_rx_queue;

enum {
	NOTIFY_PHASOR_PROCESS = 2,
	NOTIFY_TWI_PROCESS = 4
};

struct _mpc_rx_data {
	uint8_t size;
	uint8_t *buf;
};


// TODO: some form of error handling
static void mpc_nop(const mpc_pkt *const pkt) {}
static void (*cmds[MPC_CMD_MAX])(const mpc_pkt *const);

static TaskHandle_t mpc_task_handle;
static void mpc_rx_frame(uint8_t, uint8_t *);
static void mpc_task(void *params);
static inline uint8_t mpc_check_crc(const mpc_pkt *const pkt) __attribute__((always_inline));

static inline void phasor_comm_init(uint8_t);

/**
 * The shoulders need to differentiate left/right.
 */
#ifdef MPC_TWI_ADDR_EEPROM_ADDR
#include <avr/eeprom.h>
#endif

#ifdef MPC_TWI_ADDR_EEPROM_ADDR
#define __mpc_addr() \
	eeprom_read_byte((uint8_t *)MPC_TWI_ADDR_EEPROM_ADDR)
#else
#define __mpc_addr() ((uint8_t)MPC_ADDR_BOARD)
#endif

/**
 * Initialize the mpc_twi driver if this board is configured for TWI
 */
static inline void __mpc_twi_init(uint8_t mpc_addr) {
#ifdef MPC_TWI
	uint8_t mask;

	#ifdef MPC_TWI_ADDRMASK
	mask = MPC_TWI_ADDRMASK;
	#else
	mask = 0;
	#endif

	comm_dev_t *twi;
	twi = mpctwi_init(&MPC_TWI, mpc_addr, mask, MPC_TWI_BAUD);
	comm = comm_init(twi, mpc_addr, MPC_PKT_MAX_SIZE, mempool, mpc_rx_event, MPC_QUEUE_SIZE, MPC_QUEUE_SIZE);
#endif
}


void mpc_init(void) {
	uint8_t mpc_addr = __mpc_addr();

	mempool = init_mempool(MPC_PKT_MAX_SIZE + sizeof(comm_frame_t), MPC_QUEUE_SIZE);
	__mpc_twi_init(mpc_addr);

	mpc_rx_queue = xQueueCreate(MPC_QUEUE_SIZE, sizeof(struct _mpc_rx_data));
	phasor_comm_init(mpc_addr);

	for (uint8_t i = 0; i < MPC_CMD_MAX; ++i) {
		// Just in case anything calls mpc_register before mpc_init()
		if (cmds[i] == NULL)
			cmds[i] = mpc_nop;
	}

	// TODO: stack size
	xTaskCreate(mpc_task, "mpc", 256, NULL, tskIDLE_PRIORITY + 5, &mpc_task_handle);
}

// TODO
#ifdef PHASOR_COMM
static void mpc_rx_phasor_task(void *params) {
	while (1) {
		struct _mpc_rx_data rx_data = {
			.size = 0,
			.buf = NULL,
		};
		comm_frame_t *frame = mempool_alloc(mempool);

		if (frame) {
			rx_data.buf = frame->data;
		}

		rx_data.size = serialcomm_recv_frame(phasor_comm, rx_data.buf, mempool->block_size - sizeof(*frame));

		if (xQueueSend(mpc_rx_queue, &rx_data, portMAX_DELAY))
			xTaskNotify(mpc_task_handle, NOTIFY_PHASOR_PROCESS, eSetBits);
	}
}

static inline void phasor_comm_init(uint8_t mpc_addr) {
	phasor_uart_dev = uart_init(&PHASOR_COMM_USART, MPC_QUEUE_SIZE * 2, 24, PHASOR_COMM_BSEL_VALUE, PHASOR_COMM_BSCALE_VALUE);

	serialcomm_driver driver = {
		.dev = phasor_uart_dev,
		.rx_func = (uint8_t (*)(void*))uart_getchar,
		.tx_func = (void (*)(void *, uint8_t *, uint8_t, void (*)(void *)))uart_write,
	};

	phasor_comm = serialcomm_init(driver, mpc_addr);

	xTaskCreate(mpc_rx_phasor_task, "phasor-rx", 128, NULL, tskIDLE_PRIORITY + 1, (TaskHandle_t*)NULL);
}
#endif


/**
 * @TODO this will silently fail on table full
 *
 */
void mpc_register_cmd(const uint8_t cmd, void (*cb)(const mpc_pkt *const)) {
	cmds[cmd] = cb;
}

static void mpc_rx_event(comm_driver_t *evtcomm) {
	// this is dirty: TWI will notify straight from the ISR
	// But serialcomm will notify from a task.
#ifdef MPC_TWI
	if (evtcomm == comm)
		xTaskNotifyFromISR(mpc_task_handle, NOTIFY_TWI_PROCESS, eSetBits, NULL);
#endif
}

static void handle_master_hello(const mpc_pkt *const pkt) {
	if (pkt->saddr == MPC_ADDR_MASTER)
		mpc_send_cmd(MPC_ADDR_MASTER, MPC_CMD_HELLO);
}

//static void handle_master_settings(const mpc_pkt *const pkt) {
	// this space intentionally left blank for now.
//}

static void mpc_task(void *params) {
	const uint32_t all_notifications = NOTIFY_PHASOR_PROCESS | NOTIFY_PHASOR_PROCESS | NOTIFY_TWI_PROCESS;
	uint32_t notify;
	comm_frame_t *frame;

	if (!(MPC_ADDR_BOARD & MPC_ADDR_MASTER)) {
		mpc_register_cmd(MPC_CMD_HELLO, handle_master_hello);

		// TODO
		// mpc_register_cmd('S', handle_master_settings);
		mpc_send_cmd(MPC_ADDR_MASTER, MPC_CMD_HELLO);
	}

	while (1) {
		notify = 0;
		if (xTaskNotifyWait(0, all_notifications, &notify, portMAX_DELAY)) {

			#ifdef PHASOR_COMM
			if ((notify & NOTIFY_PHASOR_PROCESS)) {
				struct _mpc_rx_data rx;
				if (xQueueReceive(mpc_rx_queue, &rx, 0))
					mpc_rx_frame(rx.size, rx.buf);
			}
			#endif

			#ifdef MPC_TWI
			if ((notify & NOTIFY_TWI_PROCESS)) {
				while ((frame = comm_rx(comm)) != NULL)
					mpc_rx_frame(frame->size, frame->data);
			}
			#endif
		}
	}
}

static inline uint8_t mpc_check_crc(const mpc_pkt *const pkt) {

	if (!MPC_DISABLE_CRC) {
		const uint8_t *const data = (uint8_t*)pkt;
		uint8_t crc = mpc_crc(MPC_CRC_SHIFT, data[0]);

		for (uint8_t i = 0; i < (pkt->len + sizeof(*pkt)); ++i) {
			if (i != offsetof(mpc_pkt, chksum)) {
				crc = mpc_crc(crc, data[i] );
			}
		}

		return crc == pkt->chksum;
	} else {
		return 1;
	}
}


static void mpc_rx_frame(uint8_t size, uint8_t *buf) {
	mpc_pkt *pkt = (mpc_pkt *)buf;

	if (!mpc_check_crc(pkt)) {
		comm_putref(buf);
		return;
	}

	cmds[pkt->cmd](pkt);

	comm_putref(buf);
}

inline void mpc_send_cmd(const uint8_t addr, const uint8_t cmd) {
	mpc_send(addr, cmd, 0, NULL);
}

void mpc_send(const uint8_t addr, const uint8_t cmd, const uint8_t len, uint8_t *const data) {

	comm_frame_t *frame;
	mpc_pkt *pkt;

	frame = mempool_alloc(mempool);

	frame->daddr = addr;
	frame->size = sizeof(*pkt) + len;

	pkt = (mpc_pkt *)frame->data;

	pkt->len = len;
	pkt->cmd = cmd;

	#ifdef MPC_TWI
	pkt->saddr = comm->addr;
	#endif
	#ifdef PHASOR_COMM
	pkt->saddr = phasor_comm->addr;
	#endif

	pkt->chksum = MPC_CRC_SHIFT;

	for (uint8_t i = 0; i < sizeof(*pkt) - sizeof(pkt->chksum); ++i)
		pkt->chksum = mpc_crc(pkt->chksum, ((uint8_t *)pkt)[i]);

	for (uint8_t i = 0; i < len; ++i) {
		pkt->data[i] = data[i];
		pkt->chksum = mpc_crc(pkt->chksum, data[i]);
	}

	#ifdef PHASOR_COMM
	if (addr & (MPC_ADDR_MASTER | MPC_ADDR_PHASOR)) {
		mempool_getref(frame);
		serialcomm_send(phasor_comm, frame->daddr, frame->size, frame->data, comm_putref);
	}
	#endif

	#ifdef MPC_TWI
	if (addr & (MPC_ADDR_MASTER | MPC_ADDR_CHEST | MPC_ADDR_LS | MPC_ADDR_RS | MPC_ADDR_BACK)) {
		comm_send(comm, mempool_getref(frame));
		comm_tx(comm);
	}

	#endif

	mempool_putref(frame);
}

#ifdef MPC_TWI_MASTER_ISR
#include "twi_master.h"
MPC_TWI_MASTER_ISR {
	twi_master_isr(((mpc_twi_dev *)comm->dev->dev)->twim);
}
#endif
#ifdef MPC_TWI_SLAVE_ISR
#include "twi_slave.h"
MPC_TWI_SLAVE_ISR {
	twi_slave_isr(((mpc_twi_dev *)comm->dev->dev)->twis);
}
#endif

#ifdef PHASOR_COMM
PHASOR_COMM_TXC_ISR {
	uart_tx_isr(phasor_uart_dev);
}

PHASOR_COMM_RXC_ISR {
	uart_rx_isr(phasor_uart_dev);
}
#endif
