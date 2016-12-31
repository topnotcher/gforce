#include <stdint.h>
#include <avr/io.h>

#include "twi_master.h"
#include "twi_slave.h"
#include "mempool.h"

#include "FreeRTOS.h"
#include "queue.h"

#ifndef _MPCTWI_H
#define _MPCTWI_H

typedef struct {
	twi_master_t *twim;
	twi_slave_t *twis;
	mempool_t *mempool;
	void (*rx_callback)(uint8_t *, uint8_t);

	QueueHandle_t tx_queue;
	uint8_t *tx_buf;
} mpctwi_t;

mpctwi_t *mpctwi_init(
		TWI_t *const, const uint8_t, 
		const uint8_t, const uint8_t,
		const uint8_t tx_queue_size,
		mempool_t *, void (*)(uint8_t *, uint8_t)
);

void mpctwi_send(mpctwi_t *, uint8_t, uint8_t, uint8_t *);

static inline void mpctwi_slave_isr(mpctwi_t *mpctwi) {
	twi_slave_isr(mpctwi->twis);
}

static inline void mpctwi_master_isr(mpctwi_t *mpctwi) {
	twi_master_isr(mpctwi->twim);
}
#endif
