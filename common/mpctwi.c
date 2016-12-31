#include "mpctwi.h"

#include "malloc.h"
#include "mempool.h"

#include "FreeRTOS.h"
#include "queue.h"

struct _mpctwi_tx_data {
	uint8_t addr;
	uint8_t *buf;
	uint8_t size;
};

static void twis_txn_begin(void *, uint8_t, uint8_t **, uint8_t *);
static void twim_txn_complete(void *, int8_t);
static void twis_txn_end(void *, uint8_t, uint8_t *, uint8_t);

mpctwi_t *mpctwi_init(
		TWI_t *const twi_hw, const uint8_t addr,
		const uint8_t mask, const uint8_t baud,
		const uint8_t tx_queue_size, mempool_t *mempool,
		void (*rx_callback)(uint8_t *, uint8_t)
) {

	mpctwi_t *mpctwi = smalloc(sizeof *mpctwi);

	if (mpctwi) {
		mpctwi->twis = twi_slave_init(&twi_hw->SLAVE, addr, mask, mpctwi, twis_txn_begin, twis_txn_end);
		mpctwi->twim = twi_master_init(&twi_hw->MASTER, baud, mpctwi, twim_txn_complete);
		mpctwi->mempool = mempool;
		mpctwi->rx_callback = rx_callback;
		mpctwi->tx_buf = NULL;

		mpctwi->tx_queue = xQueueCreate(tx_queue_size, sizeof(struct _mpctwi_tx_data));
	}

	return mpctwi;
}

void mpctwi_send(mpctwi_t *mpctwi, uint8_t addr, uint8_t size, uint8_t *buf) {
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
	mpctwi_t *mpctwi = ins;
	struct _mpctwi_tx_data tx_data;

	portENTER_CRITICAL();

	mempool_putref(mpctwi->tx_buf);
	mpctwi->tx_buf = NULL;

	if (xQueueReceiveFromISR(mpctwi->tx_queue, &tx_data, NULL)) {
		mpctwi->tx_buf = tx_data.buf;
		twi_master_write(mpctwi->twim, tx_data.addr, tx_data.size, tx_data.buf);
	}

	portEXIT_CRITICAL();
}

static void twis_txn_begin(void *ins, uint8_t write, uint8_t **buf, uint8_t *buf_size) {
	/**
	 * buf may be non-null when passed (if driver has a partially used buffer)
	 * In this case, comm should still have a reference to it and will reuse it
	 * when appropriate.
	 */

	if (!write && !*buf) {
		mpctwi_t *mpctwi = ins;

		*buf = mempool_alloc(mpctwi->mempool);
		*buf_size = mpctwi->mempool->block_size;
	}
}

static void twis_txn_end(void *ins, uint8_t write, uint8_t *buf, uint8_t size) {
	mpctwi_t *mpctwi = ins;

	if (!write && buf) {
		mpctwi->rx_callback(buf, size);
	}
}
