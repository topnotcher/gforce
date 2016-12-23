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
#include <phasor_comm.h>
#include "serialcomm.h"
static comm_driver_t *phasor_comm;
#endif

#ifdef MPC_PROCESS_XBEE
#include "xbee.h"
#endif

static void handle_master_hello(const mpc_pkt *const);
//static void handle_master_settings(const mpc_pkt *const);

static mempool_t *mempool;

enum {
	NOTIFY_XBEE_PROCESS = 1,
	NOTIFY_PHASOR_PROCESS = 2,
	NOTIFY_TWI_PROCESS = 4
};


// TODO: some form of error handling
static void mpc_nop(const mpc_pkt *const pkt) {}
static void (*cmds[MPC_CMD_MAX])(const mpc_pkt *const);

static TaskHandle_t mpc_task_handle;
static void mpc_rx_frame(comm_frame_t *frame);
static void mpc_task(void *params);
static inline uint8_t mpc_check_crc(const mpc_pkt *const pkt) __attribute__((always_inline));

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
#else
#endif
}

/**
 * Initalize the mpc phasor driver if this board is phasor/master
 */
static inline void __mpc_phasor_init(uint8_t mpc_addr) {
#ifdef PHASOR_COMM
	phasor_comm = phasor_comm_init(mempool, mpc_addr, mpc_rx_event);
#else
#endif
}


void mpc_init(void) {
	uint8_t mpc_addr = __mpc_addr();

	mempool = init_mempool(MPC_PKT_MAX_SIZE + sizeof(comm_frame_t), MPC_QUEUE_SIZE);
	__mpc_twi_init(mpc_addr);
	__mpc_phasor_init(mpc_addr);

	for (uint8_t i = 0; i < MPC_CMD_MAX; ++i) {
		// Just in case anything calls mpc_register before mpc_init()
		if (cmds[i] == NULL)
			cmds[i] = mpc_nop;
	}


	xTaskCreate(mpc_task, "mpc", 128, NULL, tskIDLE_PRIORITY + 5, &mpc_task_handle);
}

/**
 * @TODO this will silently fail on table full
 *
 */
void mpc_register_cmd(const uint8_t cmd, void (*cb)(const mpc_pkt *const)) {
	cmds[cmd] = cb;
}

void mpc_rx_xbee(void) {
	xTaskNotify(mpc_task_handle, NOTIFY_XBEE_PROCESS, eSetBits);
}

static void mpc_rx_event(comm_driver_t *evtcomm) {
	// this is dirty: TWI will notify straight from the ISR
	// But serialcomm will notify from a task.
#ifdef MPC_TWI
	if (evtcomm == comm)
		xTaskNotifyFromISR(mpc_task_handle, NOTIFY_TWI_PROCESS, eSetBits, NULL);
#endif

#ifdef PHASOR_COMM
	if (evtcomm == phasor_comm)
		xTaskNotify(mpc_task_handle, NOTIFY_PHASOR_PROCESS, eSetBits);
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

			#ifdef MPC_PROCESS_XBEE
			if (notify & NOTIFY_XBEE_PROCESS)
				xbee_rx_process();
			#endif

			#ifdef PHASOR_COMM
			if ((notify & NOTIFY_PHASOR_PROCESS)) {
				while ((frame = comm_rx(phasor_comm)) != NULL)
					mpc_rx_frame(frame);
			}
			#endif

			#ifdef MPC_TWI
			if ((notify & NOTIFY_TWI_PROCESS)) {
				while ((frame = comm_rx(comm)) != NULL)
					mpc_rx_frame(frame);
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


static void mpc_rx_frame(comm_frame_t *frame) {
	mpc_pkt *pkt;

	pkt = (mpc_pkt *)frame->data;

	if (!mpc_check_crc(pkt)) {
		mempool_putref(frame);
		return;
	}

	cmds[pkt->cmd](pkt);

	//cleanup:
	mempool_putref(frame);
}

inline void mpc_send_cmd(const uint8_t addr, const uint8_t cmd) {
	mpc_send(addr, cmd, 0, NULL);
}

//CALLER MUST FREE() data*
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
		phasor_comm_send(phasor_comm, frame->daddr, frame->size, frame->data);
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
