#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

#include <saml21/io.h>

#include <drivers/saml21/sercom.h>
#include <drivers/saml21/twi/slave.h>
#include <malloc.h>

static void twi_slave_handler(void *);

static bool twi_slave_direction(const Sercom *const _sercom) {
	// i2c direction bit: 1 indicates master read (slave sends data to master)
	return _sercom->I2CS.STATUS.reg & SERCOM_I2CS_STATUS_DIR;
}

twi_slave_t *twi_slave_init(void) {
	Sercom *sercom = SERCOM3;
	int sercom_index = sercom_get_index(sercom);

	twi_slave_t *dev = smalloc(sizeof *dev);

	if (dev) {
		dev->sercom = sercom;

		// TODO: Not true for SERCOM5, which is on AHB-ABP bridge D.
		int pm_index = sercom_index + MCLK_APBCMASK_SERCOM0_Pos;
		int gclk_index = sercom_index + SERCOM0_GCLK_ID_CORE;

		MCLK->APBCMASK.reg |= 1 << pm_index;

		// disable peripheral clock channel
		GCLK->PCHCTRL[gclk_index].reg &= ~GCLK_PCHCTRL_CHEN;
		while (GCLK->PCHCTRL[gclk_index].reg & GCLK_PCHCTRL_CHEN);

		// set peripheral clock thannel generator to gclk0
		GCLK->PCHCTRL[gclk_index].reg = GCLK_PCHCTRL_GEN_GCLK0;

		GCLK->PCHCTRL[gclk_index].reg |= GCLK_PCHCTRL_CHEN;
		while (!(GCLK->PCHCTRL[gclk_index].reg & GCLK_PCHCTRL_CHEN));

		/**
		* Enable slow clock
		*/
		// disable peripheral clock channel
		GCLK->PCHCTRL[SERCOM3_GCLK_ID_CORE].reg &= ~GCLK_PCHCTRL_CHEN;
		while (GCLK->PCHCTRL[SERCOM3_GCLK_ID_CORE].reg & GCLK_PCHCTRL_CHEN);

		// set peripheral clock thannel generator to gclk0
		GCLK->PCHCTRL[SERCOM3_GCLK_ID_SLOW].reg = GCLK_PCHCTRL_GEN_GCLK0;

		GCLK->PCHCTRL[SERCOM3_GCLK_ID_SLOW].reg |= GCLK_PCHCTRL_CHEN;
		while (!(GCLK->PCHCTRL[SERCOM3_GCLK_ID_SLOW].reg & GCLK_PCHCTRL_CHEN));

		// Disable module
		// Parts of CTRLA and CTRLB are enabled-protected
		sercom->I2CS.CTRLA.reg &= ~SERCOM_I2CS_CTRLA_ENABLE;
		while (sercom->I2CS.SYNCBUSY.reg & SERCOM_I2CS_SYNCBUSY_ENABLE);

		uint32_t ctrla = SERCOM_I2CS_CTRLA_MODE(0x04);
		uint32_t ctrlb = 0;

		// set MODE
		sercom->I2CS.CTRLA.reg = ctrla;

		// 0x00 = mask: responds to ADDR.ADDR masked by ADDR.ADDRMASK (bits masked
		// out in ADDRMASK do not count)
		ctrlb |= SERCOM_I2CS_CTRLB_AMODE(0x00);
		sercom->I2CS.ADDR.reg = SERCOM_I2CS_ADDR_ADDR(0x02) | SERCOM_I2CS_ADDR_ADDRMASK(~0x02U);

		sercom->I2CS.CTRLA.reg = ctrla;
		sercom->I2CS.CTRLB.reg = ctrlb;

		sercom->I2CS.CTRLA.reg |= SERCOM_I2CS_CTRLA_ENABLE;
		while (sercom->I2CS.SYNCBUSY.reg & SERCOM_I2CS_SYNCBUSY_ENABLE);

		sercom->I2CS.INTENSET.reg =
			SERCOM_I2CS_INTENSET_PREC |
			SERCOM_I2CS_INTENSET_AMATCH |
			SERCOM_I2CS_INTENSET_DRDY |
			SERCOM_I2CS_INTENSET_ERROR;

		sercom_register_handler(sercom_index, twi_slave_handler, dev);
		sercom_enable_irq(sercom_index);

		// TODO WRCONFIG
		// Each pin is 1 nybble (even pin = low); set them both to D (0x04)
		uint8_t pmux = 0x02 | (0x02 << 4);
		PORT[0].Group[0].PMUX[11].reg = pmux;

		PORT[0].Group[0].PINCFG[22].reg |= PORT_PINCFG_PMUXEN;
		PORT[0].Group[0].PINCFG[23].reg |= PORT_PINCFG_PMUXEN;
	}

	return dev;
}

static void twi_slave_handler(void *_isr_ins) {
	twi_slave_t *dev = _isr_ins;
	Sercom *sercom = dev->sercom;

	// TODO SMEN: smart mode - auto ack on DATA read
	// TODO AACKEN: auto ack on address match.

	// SERCOM_I2CS_CTRLB_ACKACT = 0: send ack; 1 send nak

	// Address match
	if (sercom->I2CS.INTFLAG.reg & SERCOM_I2CS_INTFLAG_AMATCH) {
		dev->begin_txn(dev->ins, twi_slave_direction(sercom), &dev->buf, &dev->buf_size);

		if (dev->buf) {
			dev->bytes = 0;
			sercom->I2CS.CTRLB.reg &= ~SERCOM_I2CS_CTRLB_ACKACT;
		} else {
			sercom->I2CS.CTRLB.reg |= SERCOM_I2CS_CTRLB_ACKACT;
		}

		// execute ackact
		sercom->I2CS.INTFLAG.reg = SERCOM_I2CS_INTFLAG_AMATCH;

	// Data ready: flag cleared when writing to the data register, writing a
	// command to the command register, or reading the data register when SMEN is set.
	} else if (sercom->I2CS.INTFLAG.reg & SERCOM_I2CS_INTFLAG_DRDY) {

		if (dev->bytes < dev->buf_size) {
			// slave write
			if (twi_slave_direction(sercom)) {
				sercom->I2CS.DATA.reg = dev->buf[dev->bytes++];

			//slave read (master write)
			} else {
				dev->buf[dev->bytes++] = sercom->I2CS.DATA.reg;
			}
		}

		// TODO: does ackact matter here?
		sercom->I2CS.CTRLB.reg &= ~SERCOM_I2CS_CTRLB_ACKACT;
		sercom->I2CS.CTRLB.reg |= SERCOM_I2CS_CTRLB_CMD(0x03);
	} else if (sercom->I2CS.INTFLAG.reg & SERCOM_I2CS_INTFLAG_PREC) {
		dev->end_txn(dev->ins, twi_slave_direction(sercom), dev->buf, dev->bytes);
		dev->buf_size = 0;
		dev->buf = NULL;

		sercom->I2CS.INTFLAG.reg = SERCOM_I2CS_INTFLAG_PREC; 
	} else if (sercom->I2CS.INTFLAG.reg & SERCOM_I2CS_INTFLAG_ERROR) {
		sercom->I2CS.INTFLAG.reg = SERCOM_I2CS_INTENSET_ERROR;
	}
}
