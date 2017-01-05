#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include <util/crc16.h>

#include "g4config.h"
#include "config.h"
#include "mpc.h"
#include "util.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#define mpc_crc(crc, data) ((MPC_DISABLE_CRC) ? 0 : _crc_ibutton_update(crc, data))

#define MAX_MPC_DRIVERS 2
static struct {
	uint8_t addr;
	mempool_t *mempool;
	QueueHandle_t rx_queue;
	mpc_driver_t *drivers[MAX_MPC_DRIVERS];
} mpc_monostate;

static void handle_master_hello(const mpc_pkt *const);
//static void handle_master_settings(const mpc_pkt *const);

// TODO: some form of error handling
static void mpc_nop(const mpc_pkt *pkt) {}
static void (*cmds[MPC_CMD_MAX])(const mpc_pkt *);

static void mpc_rx_frame(uint8_t, uint8_t *);
static void mpc_task(void *params);
static bool mpc_check_crc(const mpc_pkt *pkt);
static uint8_t *mpc_alloc(uint8_t *);

bool mpc_register_driver(mpc_driver_t *driver) {
	bool status = false;

	if (driver != NULL) {
		mpc_driver_t **cur = mpc_monostate.drivers;

		while (*cur) ++cur;

		if (*cur == NULL) {
			*cur = driver;

			// TODO: meeeh
			mpc_monostate.addr = driver->addr;

			driver->tx_complete = mempool_putref;
			driver->alloc_buf = mpc_alloc;
			driver->rx_queue = mpc_monostate.rx_queue;

			status = driver->registered(driver);

			if (!status)
				*cur = NULL;
		}
	}

	return status;
}

void mpc_init(void) {
	memset(&mpc_monostate, 0, sizeof(mpc_monostate));

	mpc_monostate.mempool = init_mempool(MPC_PKT_MAX_SIZE, MPC_QUEUE_SIZE);
	mpc_monostate.rx_queue = xQueueCreate(MPC_QUEUE_SIZE, sizeof(struct mpc_rx_data));

	for (uint8_t i = 0; i < MPC_CMD_MAX; ++i) {
		// Just in case anything calls mpc_register before mpc_init()
		if (cmds[i] == NULL)
			cmds[i] = mpc_nop;
	}

	xTaskCreate(mpc_task, "mpc", 128, NULL, tskIDLE_PRIORITY + 5, NULL);
}

static uint8_t *mpc_alloc(uint8_t *const size) {
	*size = mpc_monostate.mempool->block_size;
	return mempool_alloc(mpc_monostate.mempool);
}

void mpc_register_cmd(const uint8_t cmd, void (*cb)(const mpc_pkt *const)) {
	cmds[cmd] = cb;
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
		struct mpc_rx_data rx;
		if (xQueueReceive(mpc_monostate.rx_queue, &rx, portMAX_DELAY))
			mpc_rx_frame(rx.size, rx.buf);
	}
}

static bool mpc_check_crc(const mpc_pkt *const pkt) {

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

void mpc_send(const uint8_t addr, const uint8_t cmd, const uint8_t len, uint8_t *const data) {
	mpc_pkt *pkt;

	if ((pkt = mempool_alloc(mpc_monostate.mempool))) {

		pkt->len = len;
		pkt->cmd = cmd;
		pkt->saddr = mpc_monostate.addr;
		pkt->chksum = MPC_CRC_SHIFT;

		for (uint8_t i = 0; i < sizeof(*pkt) - sizeof(pkt->chksum); ++i)
			pkt->chksum = mpc_crc(pkt->chksum, ((uint8_t *)pkt)[i]);

		for (uint8_t i = 0; i < len; ++i) {
			pkt->data[i] = data[i];
			pkt->chksum = mpc_crc(pkt->chksum, data[i]);
		}

		for (uint8_t i = 0; i < MAX_MPC_DRIVERS && mpc_monostate.drivers[i];  ++i) {
			mpc_driver_t *driver = mpc_monostate.drivers[i];
			if (addr & (driver)->addrmask)
				(driver)->tx(driver, addr, sizeof(*pkt) + pkt->len, mempool_getref(pkt));
		}

		mempool_putref(pkt);
	}
}

extern inline void mpc_decref(const mpc_pkt *);
extern inline void mpc_incref(const mpc_pkt *);
