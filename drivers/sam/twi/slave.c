#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <sam.h>

#include <drivers/sam/sercom.h>
#include <drivers/sam/twi/slave.h>
#include <malloc.h>

struct _twi_slave_s {
	sercom_t sercom;
	void *ins;

	twi_slave_begin_txn begin_txn;
	twi_slave_end_txn end_txn;

	uint8_t *buf;
	uint8_t buf_size;
	uint8_t bytes;
	uint8_t addr;
};

static void twi_slave_handler(sercom_t *);
static inline bool twi_slave_direction(const Sercom *) __attribute__((always_inline));

// TODO: this mask has a different meaning than on the XMEGA
twi_slave_t *twi_slave_init(const int sercom_index, uint8_t addr, uint8_t mask){
	twi_slave_t *dev = smalloc(sizeof *dev);

	if (dev != NULL) {
		memset(dev, 0, sizeof(*dev));
	}

	if (dev != NULL && sercom_init(sercom_index, &dev->sercom)) {

		dev->begin_txn = NULL;
		dev->end_txn = NULL;
		dev->ins = NULL;

		sercom_set_gclk_core(&dev->sercom, GCLK_PCHCTRL_GEN_GCLK0);
		sercom_set_gclk_slow(&dev->sercom, GCLK_PCHCTRL_GEN_GCLK0);

		Sercom *const hw = dev->sercom.hw;

		// Disable module
		// Parts of CTRLA and CTRLB are enabled-protected
		hw->I2CS.CTRLA.reg &= ~SERCOM_I2CS_CTRLA_ENABLE;
		while (hw->I2CS.SYNCBUSY.reg & SERCOM_I2CS_SYNCBUSY_ENABLE);

		uint32_t ctrla = SERCOM_I2CS_CTRLA_MODE(0x04);
		uint32_t ctrlb = 0;

		// set MODE
		hw->I2CS.CTRLA.reg = ctrla;

		// 0x00 = mask: responds to ADDR.ADDR masked by ADDR.ADDRMASK (bits masked
		// out in ADDRMASK do not count)
		ctrlb |= SERCOM_I2CS_CTRLB_AMODE(0x00);
		hw->I2CS.ADDR.reg = SERCOM_I2CS_ADDR_ADDR(addr) | SERCOM_I2CS_ADDR_ADDRMASK(mask);

		hw->I2CS.CTRLA.reg = ctrla;
		hw->I2CS.CTRLB.reg = ctrlb;

		hw->I2CS.CTRLA.reg |= SERCOM_I2CS_CTRLA_ENABLE;
		while (hw->I2CS.SYNCBUSY.reg & SERCOM_I2CS_SYNCBUSY_ENABLE);

		hw->I2CS.INTENSET.reg =
			SERCOM_I2CS_INTENSET_PREC |
			SERCOM_I2CS_INTENSET_AMATCH |
			SERCOM_I2CS_INTENSET_DRDY |
			SERCOM_I2CS_INTENSET_ERROR;

		sercom_register_handler(&dev->sercom, SERCOM_I2CS_INTFLAG_AMATCH_Pos, twi_slave_handler);
		sercom_enable_irq(&dev->sercom, SERCOM_I2CS_INTFLAG_AMATCH_Pos);

		sercom_register_handler(&dev->sercom, SERCOM_I2CS_INTFLAG_DRDY_Pos, twi_slave_handler);
		sercom_enable_irq(&dev->sercom, SERCOM_I2CS_INTFLAG_DRDY_Pos);

		sercom_register_handler(&dev->sercom, SERCOM_I2CS_INTFLAG_DRDY_Pos, twi_slave_handler);
		sercom_enable_irq(&dev->sercom, SERCOM_I2CS_INTFLAG_DRDY_Pos);

		sercom_register_handler(&dev->sercom, SERCOM_I2CS_INTFLAG_PREC_Pos, twi_slave_handler);
		sercom_enable_irq(&dev->sercom, SERCOM_I2CS_INTFLAG_PREC_Pos);

		sercom_register_handler(&dev->sercom, SERCOM_I2CS_INTFLAG_ERROR_Pos, twi_slave_handler);
		sercom_enable_irq(&dev->sercom, SERCOM_I2CS_INTFLAG_ERROR_Pos);

	}

	return dev;
}

static void twi_slave_handler(sercom_t *s) {
	twi_slave_t *dev = from_sercom(dev, s, sercom);
	Sercom *hw = s->hw;

	// TODO SMEN: smart mode - auto ack on DATA read
	// TODO AACKEN: auto ack on address match.

	// SERCOM_I2CS_CTRLB_ACKACT = 0: send ack; 1 send nak

	// Address match
	if (hw->I2CS.INTFLAG.reg & SERCOM_I2CS_INTFLAG_AMATCH) {
		if (dev->begin_txn)
			dev->begin_txn(dev->ins, twi_slave_direction(hw), &dev->buf, &dev->buf_size);

		if (dev->buf) {
			dev->bytes = 0;
			hw->I2CS.CTRLB.reg &= ~SERCOM_I2CS_CTRLB_ACKACT;
		} else {
			hw->I2CS.CTRLB.reg |= SERCOM_I2CS_CTRLB_ACKACT;
		}

		// execute ackact
		hw->I2CS.INTFLAG.reg = SERCOM_I2CS_INTFLAG_AMATCH;

	// Data ready: flag cleared when writing to the data register, writing a
	// command to the command register, or reading the data register when SMEN is set.
	} else if (hw->I2CS.INTFLAG.reg & SERCOM_I2CS_INTFLAG_DRDY) {

		if (dev->bytes < dev->buf_size) {
			// slave write
			if (twi_slave_direction(hw)) {
				hw->I2CS.DATA.reg = dev->buf[dev->bytes++];

			//slave read (master write)
			} else {
				dev->buf[dev->bytes++] = hw->I2CS.DATA.reg;
			}
		}

		// TODO: does ackact matter here?
		hw->I2CS.CTRLB.reg &= ~SERCOM_I2CS_CTRLB_ACKACT;
		hw->I2CS.CTRLB.reg |= SERCOM_I2CS_CTRLB_CMD(0x03);
	} else if (hw->I2CS.INTFLAG.reg & SERCOM_I2CS_INTFLAG_PREC) {
		if (dev->end_txn)
			dev->end_txn(dev->ins, twi_slave_direction(hw), dev->buf, dev->bytes);

		dev->buf_size = 0;
		dev->buf = NULL;

		hw->I2CS.INTFLAG.reg = SERCOM_I2CS_INTFLAG_PREC;
	} else if (hw->I2CS.INTFLAG.reg & SERCOM_I2CS_INTFLAG_ERROR) {
		hw->I2CS.INTFLAG.reg = SERCOM_I2CS_INTENSET_ERROR;
	}
}

void twi_slave_set_callbacks(twi_slave_t *dev, void *ins, twi_slave_begin_txn begin_txn, twi_slave_end_txn end_txn) {
	if (dev) {
		dev->ins = ins;
		dev->begin_txn = begin_txn;
		dev->end_txn = end_txn;
	}
}

static inline bool twi_slave_direction(const Sercom *const _sercom) {
	// i2c direction bit: 1 indicates master read (slave sends data to master)
	return _sercom->I2CS.STATUS.reg & SERCOM_I2CS_STATUS_DIR;
}
