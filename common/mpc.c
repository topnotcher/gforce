#include <stddef.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/crc16.h>

#include <g4config.h>
#include "config.h"
#include <mpc.h>
#include <util.h>

#include "mpctwi.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#define mpc_crc(crc, data) ((MPC_DISABLE_CRC) ? 0 : _crc_ibutton_update(crc, data))

static void mpc_rx_event(uint8_t *, uint8_t);

#ifdef MPC_TWI
static mpctwi_t *mpctwi;
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
static uint8_t board_mpc_addr;

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

	mpctwi = mpctwi_init(&MPC_TWI, mpc_addr, mask, MPC_TWI_BAUD, MPC_QUEUE_SIZE, mempool, mpc_rx_event);
#endif
}

void mpc_init(void) {
	board_mpc_addr = __mpc_addr();

	mempool = init_mempool(MPC_PKT_MAX_SIZE, MPC_QUEUE_SIZE);
	__mpc_twi_init(board_mpc_addr);

	mpc_rx_queue = xQueueCreate(MPC_QUEUE_SIZE, sizeof(struct _mpc_rx_data));
	phasor_comm_init(board_mpc_addr);

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
		void *buf = mempool_alloc(mempool);

		if (buf) {
			rx_data.buf = buf;
		}

		rx_data.size = serialcomm_recv_frame(phasor_comm, rx_data.buf, mempool->block_size);

		xQueueSend(mpc_rx_queue, &rx_data, portMAX_DELAY);
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

static void mpc_rx_event(uint8_t *buf, uint8_t size) {
	// this is dirty: TWI will notify straight from the ISR
	// But serialcomm will notify from a task.
#ifdef MPC_TWI
	struct _mpc_rx_data rx_data = {
		.size = size,
		.buf = buf,
	};
	xQueueSendFromISR(mpc_rx_queue, &rx_data, NULL);
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
	if (!(MPC_ADDR_BOARD & MPC_ADDR_MASTER)) {
		mpc_register_cmd(MPC_CMD_HELLO, handle_master_hello);

		// TODO
		// mpc_register_cmd('S', handle_master_settings);
		mpc_send_cmd(MPC_ADDR_MASTER, MPC_CMD_HELLO);
	}

	while (1) {
		struct _mpc_rx_data rx;
		if (xQueueReceive(mpc_rx_queue, &rx, portMAX_DELAY))
			mpc_rx_frame(rx.size, rx.buf);
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
		mempool_putref(buf);
		return;
	}

	cmds[pkt->cmd](pkt);

	mempool_putref(buf);
}

inline void mpc_send_cmd(const uint8_t addr, const uint8_t cmd) {
	mpc_send(addr, cmd, 0, NULL);
}

void mpc_send(const uint8_t addr, const uint8_t cmd, const uint8_t len, uint8_t *const data) {
	mpc_pkt *pkt;

	if ((pkt = mempool_alloc(mempool))) {

		pkt->len = len;
		pkt->cmd = cmd;
		pkt->saddr = board_mpc_addr;
		pkt->chksum = MPC_CRC_SHIFT;

		for (uint8_t i = 0; i < sizeof(*pkt) - sizeof(pkt->chksum); ++i)
			pkt->chksum = mpc_crc(pkt->chksum, ((uint8_t *)pkt)[i]);

		for (uint8_t i = 0; i < len; ++i) {
			pkt->data[i] = data[i];
			pkt->chksum = mpc_crc(pkt->chksum, data[i]);
		}

		#ifdef PHASOR_COMM
		if (addr & (MPC_ADDR_MASTER | MPC_ADDR_PHASOR)) {
			serialcomm_send(phasor_comm, addr, sizeof(*pkt) + pkt->len, mempool_getref(pkt), mempool_putref);
		}
		#endif

		#ifdef MPC_TWI
		if (addr & (MPC_ADDR_MASTER | MPC_ADDR_CHEST | MPC_ADDR_LS | MPC_ADDR_RS | MPC_ADDR_BACK)) {
			mpctwi_send(mpctwi, addr, sizeof(*pkt) + pkt->len, mempool_getref(pkt));
		}
		#endif

		mempool_putref(pkt);
	}
}

#ifdef MPC_TWI_MASTER_ISR
MPC_TWI_MASTER_ISR {
	mpctwi_master_isr(mpctwi);
}
#endif
#ifdef MPC_TWI_SLAVE_ISR
MPC_TWI_SLAVE_ISR {
	mpctwi_slave_isr(mpctwi);
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
