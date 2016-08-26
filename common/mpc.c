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

#if !defined(MPC_ADDR) || MPC_ADDR != MPC_MASTER_ADDR
static void handle_master_hello(const mpc_pkt *const);
//static void handle_master_settings(const mpc_pkt *const);
#endif

static mempool_t *mempool;

//@TODO hard-coded # of elementsghey
#define N_MPC_CMDS 15
typedef struct {
	uint8_t cmd;
	void (*cb)(const mpc_pkt *const);
} cmd_callback_s;

enum {
	NOTIFY_XBEE_PROCESS = 1,
	NOTIFY_PHASOR_PROCESS = 2,
	NOTIFY_TWI_PROCESS = 4
};

static cmd_callback_s cmds[N_MPC_CMDS];
static uint8_t next_mpc_cmd = 0;
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
#define __mpc_addr() ((uint8_t)MPC_ADDR)
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
	comm = comm_init(twi, mpc_addr, MPC_PKT_MAX_SIZE, mempool, mpc_rx_event);
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

	xTaskCreate(mpc_task, "mpc", 128, NULL, tskIDLE_PRIORITY + 5, &mpc_task_handle);
}

/**
 * @TODO this will silently fail on table full
 *
 */
void mpc_register_cmd(const uint8_t cmd, void (*cb)(const mpc_pkt *const)) {
	cmds[next_mpc_cmd].cmd = cmd;
	cmds[next_mpc_cmd].cb = cb;
	next_mpc_cmd++;
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

#if !defined(MPC_ADDR) || MPC_ADDR != MPC_MASTER_ADDR
static void handle_master_hello(const mpc_pkt *const pkt) {
	if (pkt->saddr == MPC_MASTER_ADDR)
		mpc_send_cmd(MPC_MASTER_ADDR, 'H');
}

//static void handle_master_settings(const mpc_pkt *const pkt) {
	// this space intentionally left blank for now.
//}
#endif

static void mpc_task(void *params) {
	const uint32_t all_notifications = NOTIFY_PHASOR_PROCESS | NOTIFY_PHASOR_PROCESS | NOTIFY_TWI_PROCESS;
	uint32_t notify;
	comm_frame_t *frame;

#if !defined(MPC_ADDR) || MPC_ADDR != MPC_MASTER_ADDR
		mpc_register_cmd('H', handle_master_hello);
		// TODO
		// mpc_register_cmd('S', handle_master_settings);
		mpc_send_cmd(MPC_MASTER_ADDR, 'H');
#endif

	while (1) {
		notify = 0;
		if (xTaskNotifyWait(0, all_notifications, &notify, portMAX_DELAY)) {

			#ifdef MPC_PROCESS_XBEE
			if (notify & NOTIFY_XBEE_PROCESS)
				xbee_rx_process();
			#endif

			#ifdef PHASOR_COMM
			if ((notify & NOTIFY_PHASOR_PROCESS) && ((frame = comm_rx(phasor_comm)) != NULL))
				mpc_rx_frame(frame);
			#endif

			#ifdef MPC_TWI
			if ((notify & NOTIFY_TWI_PROCESS) && ((frame = comm_rx(comm)) != NULL))
				mpc_rx_frame(frame);
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

	for (uint8_t i = 0; i < next_mpc_cmd; ++i) {
		if (cmds[i].cmd == pkt->cmd) {
			cmds[i].cb(pkt);
			break;
		}
	}

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
	if (addr & (MPC_MASTER_ADDR | MPC_PHASOR_ADDR)) {
		comm_send(phasor_comm, mempool_getref(frame));
		comm_tx(phasor_comm);
	}
	#endif

	#ifdef MPC_TWI
	if (addr & (MPC_MASTER_ADDR | MPC_CHEST_ADDR | MPC_LS_ADDR | MPC_RS_ADDR | MPC_BACK_ADDR)) {
		comm_send(comm, mempool_getref(frame));
		comm_tx(comm);
	}

	#endif

	mempool_putref(frame);
}

#ifdef PHASOR_COMM_TXC_ISR
PHASOR_COMM_TXC_ISR {
	serialcomm_tx_isr(phasor_comm);
}
#endif

#ifdef PHASOR_COMM_RXC_ISR
PHASOR_COMM_RXC_ISR {
	serialcomm_rx_isr(phasor_comm);
}
#endif

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
