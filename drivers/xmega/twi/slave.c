#include <string.h>
#include <stdbool.h>
#include <stddef.h>

#include <drivers/xmega/twi/slave.h>
#include <malloc.h>

struct _twi_slave_s {
	TWI_SLAVE_t *twi;
	uint8_t addr;
	void *ins;

	uint8_t *buf;
	uint8_t buf_size;
	uint8_t bytes;

	twi_slave_begin_txn begin_txn;
	twi_slave_end_txn end_txn;
};

static inline bool twi_slave_direction(const TWI_SLAVE_t *) __attribute__((always_inline));

twi_slave_t *twi_slave_init(TWI_SLAVE_t *twi, uint8_t addr, uint8_t mask) {
	twi_slave_t *dev;
	dev = (twi_slave_t *)smalloc(sizeof *dev);
	memset(dev, 0, sizeof *dev);

	dev->twi = twi;
	dev->addr = addr << 1;
	dev->buf = NULL;
	dev->bytes = 0;
	dev->buf_size = 0;

	twi->CTRLA = TWI_SLAVE_INTLVL_LO_gc | TWI_SLAVE_ENABLE_bm | TWI_SLAVE_APIEN_bm /*| TWI_SLAVE_PMEN_bm*/;
	twi->ADDR = dev->addr;

	if (mask)
		twi->ADDRMASK = mask << 1;

	return dev;
}

void twi_slave_isr(twi_slave_t *dev) {
	TWI_SLAVE_t *twi = (TWI_SLAVE_t *)dev->twi;

	if ((twi->STATUS & TWI_SLAVE_APIF_bm) && (twi->STATUS & TWI_SLAVE_AP_bm)) {
		uint8_t addr = twi->DATA;
		/*@TODO: repeated starts are not handled*/
		if (addr & dev->addr) {
			uint8_t write = twi_slave_direction(twi);

			dev->begin_txn(dev->ins, write, &dev->buf, &dev->buf_size);

			if (dev->buf) {
				dev->bytes = 0;
				twi->CTRLA |= TWI_SLAVE_DIEN_bm | TWI_SLAVE_PIEN_bm;
				twi->CTRLB = TWI_SLAVE_CMD_RESPONSE_gc;
			} else {
				//is this right?
				twi->CTRLB = TWI_SLAVE_CMD_COMPTRANS_gc;
			}
		} else {
			//is this right?
			twi->CTRLB = TWI_SLAVE_CMD_COMPTRANS_gc;
		}
	} else if (twi->STATUS & TWI_SLAVE_DIF_bm) {
		// slave write
		if (twi->STATUS & TWI_SLAVE_DIR_bm) {
			//@TODO handle overflow with error
			if (dev->bytes < dev->buf_size)
				twi->DATA = dev->buf[dev->bytes++];

			//slave read(master write)
		} else {
			//@TODO handle overflow with error
			if (dev->bytes < dev->buf_size)
				dev->buf[dev->bytes++] = twi->DATA;
		}
		//@TODO SMART MODE
		twi->CTRLB = TWI_SLAVE_CMD_RESPONSE_gc;

		//stop interrupt
	} else if (twi->STATUS & TWI_SLAVE_APIF_bm) {
		dev->end_txn(dev->ins, twi_slave_direction(twi), dev->buf, dev->bytes);
		dev->buf_size = 0;
		dev->buf = NULL;

		twi->CTRLA &= ~(TWI_SLAVE_PIEN_bm | TWI_SLAVE_DIEN_bm);
		twi->STATUS |= TWI_SLAVE_APIF_bm;
	} else {
		//Some kind of unexpected interrupt
	}
}

void twi_slave_set_callbacks(twi_slave_t *dev, void *ins, twi_slave_begin_txn begin_txn, twi_slave_end_txn end_txn) {
	if (dev) {
		dev->ins = ins;
		dev->begin_txn = begin_txn;
		dev->end_txn = end_txn;
	}
}

static inline bool twi_slave_direction(const TWI_SLAVE_t *twi) {
	return twi->STATUS & TWI_SLAVE_DIR_bm;
}
