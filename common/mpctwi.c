#include <mpctwi.h>

#include <malloc.h>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <g4config.h>
#include "config.h"

struct _mpctwi_tx_data {
	uint8_t addr;
	uint8_t *buf;
	uint8_t size;
};

typedef struct {
	twi_master_t *twim;
	twi_slave_t *twis;

	QueueHandle_t tx_queue;
	uint8_t *tx_buf;
} mpctwi_t;

static void twis_txn_begin(void *, uint8_t, uint8_t **, uint8_t *);
static void twim_txn_complete(void *, int8_t);
static void twis_txn_end(void *, uint8_t, uint8_t *, uint8_t);
static bool mpctwi_registered(mpc_driver_t *);

mpc_driver_t *mpctwi_init(twi_master_t *twim, twi_slave_t *twis, uint8_t addr, uint8_t tx_addrmask) {
	mpctwi_t *_mpctwi = NULL;
	mpc_driver_t *driver = NULL;

	if ((driver = smalloc(sizeof *driver)) && (_mpctwi = smalloc(sizeof *_mpctwi))) {
		driver->ins = _mpctwi;
		driver->addr = addr;
		driver->addrmask = tx_addrmask;
		driver->tx = mpctwi_send;
		driver->registered = mpctwi_registered;

		_mpctwi->twis = twis;
		twi_slave_set_callbacks(_mpctwi->twis, driver, twis_txn_begin, twis_txn_end);

		_mpctwi->twim = twim;
		twi_master_set_callback(_mpctwi->twim, driver, twim_txn_complete);

		if (_mpctwi->twis == NULL || _mpctwi->twim == NULL)
			driver = NULL;
	}

	return driver;
}

static bool mpctwi_registered(mpc_driver_t *driver) {
	if (driver && driver->ins) {
		mpctwi_t *mpctwi = driver->ins;

		mpctwi->tx_buf = NULL;
		mpctwi->tx_queue = xQueueCreate(MPC_QUEUE_SIZE, sizeof(struct _mpctwi_tx_data));

		return true;
	}

	return false;
}

void mpctwi_send(mpc_driver_t *driver, uint8_t addr, uint8_t size, uint8_t *buf) {
	mpctwi_t *mpctwi = driver->ins;

	portENTER_CRITICAL();

	if (mpctwi->tx_buf) {
		struct _mpctwi_tx_data tx_data  = {
			.addr = addr,
			.buf = buf,
			.size = size
		};

		xQueueSend(mpctwi->tx_queue, &tx_data, 0);

	} else {
		mpctwi->tx_buf = buf;
		twi_master_write(mpctwi->twim, addr, size, buf);
	}
	portEXIT_CRITICAL();
}

static void twim_txn_complete(void *ins, int8_t status) {
	mpc_driver_t *driver = ins;
	mpctwi_t *mpctwi = driver->ins;
	struct _mpctwi_tx_data tx_data;

	portENTER_CRITICAL();

	if (mpctwi->tx_buf)
		driver->tx_complete(mpctwi->tx_buf);

	mpctwi->tx_buf = NULL;

	if (xQueueReceiveFromISR(mpctwi->tx_queue, &tx_data, NULL)) {
		mpctwi->tx_buf = tx_data.buf;
		twi_master_write(mpctwi->twim, tx_data.addr, tx_data.size, tx_data.buf);
	}

	portEXIT_CRITICAL();
}

static void twis_txn_begin(void *ins, uint8_t write, uint8_t **buf, uint8_t *buf_size) {
	mpc_driver_t *driver = ins;

	/**
	 * buf may be non-null when passed (if driver has a partially used buffer)
	 * In this case, comm should still have a reference to it and will reuse it
	 * when appropriate.
	 */

	if (!write && !*buf) {
		*buf = driver->alloc_buf(buf_size);
	}
}

static void twis_txn_end(void *ins, uint8_t write, uint8_t *buf, uint8_t size) {
	mpc_driver_t *driver = ins;

	if (!write && buf) {
		struct mpc_rx_data rx_data = {
			.size = size,
			.buf = buf,
		};

		xQueueSendFromISR(driver->rx_queue, &rx_data, NULL);
	}
}
