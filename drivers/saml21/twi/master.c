#include <stdint.h>
#include <string.h>

#include <malloc.h>
#include <saml21/io.h>

#include <drivers/saml21/sercom.h>
#include <drivers/saml21/twi/master.h>

struct _twi_master_s {
	Sercom *sercom;
	twi_master_complete_txn_cb txn_complete;
	void *ins;
	uint8_t *txbuf;
	uint8_t *rxbuf;

	uint8_t rxbytes;
	uint8_t txbytes;

	uint8_t bytes;
	uint8_t retries;

	// currently addressed slave
	uint8_t addr;
};

static void twi_master_start_txn(const twi_master_t *);
static void twi_master_txn_complete(twi_master_t *, int8_t);
static void twi_master_isr(void *);

twi_master_t *twi_master_init(Sercom *sercom, const const uint8_t baud) {
	int sercom_index = sercom_get_index(sercom);
	twi_master_t *dev = smalloc(sizeof *dev);

	if (dev) {
		memset(dev, 0, sizeof(*dev));

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
		// TODO: all of the SERCOMs (except sercom5?) share a slow clock - this
		// only needs to be done once...
		// disable peripheral clock channel
		/*GCLK->PCHCTRL[SERCOM1_GCLK_ID_CORE].reg &= ~GCLK_PCHCTRL_CHEN;
		while (GCLK->PCHCTRL[SERCOM1_GCLK_ID_CORE].reg & GCLK_PCHCTRL_CHEN);

		// set peripheral clock thannel generator to gclk0
		GCLK->PCHCTRL[SERCOM1_GCLK_ID_SLOW].reg = GCLK_PCHCTRL_GEN_GCLK0;

		GCLK->PCHCTRL[SERCOM1_GCLK_ID_SLOW].reg |= GCLK_PCHCTRL_CHEN;
		while (!(GCLK->PCHCTRL[SERCOM1_GCLK_ID_SLOW].reg & GCLK_PCHCTRL_CHEN));
		*/

		// Disable module
		// Parts of CTRLA and CTRLB are enabled-protected
		sercom->I2CM.CTRLA.reg &= ~SERCOM_I2CM_CTRLA_ENABLE;
		while (sercom->I2CM.SYNCBUSY.reg & SERCOM_I2CM_SYNCBUSY_ENABLE);

		uint32_t ctrla = SERCOM_I2CM_CTRLA_MODE(0x05);
		uint32_t ctrlb = 0;

		// set MODE
		sercom->I2CM.CTRLA.reg = ctrla;

		sercom->I2CM.CTRLA.reg = ctrla;
		sercom->I2CM.CTRLB.reg = ctrlb;
		sercom->I2CM.STATUS.reg = SERCOM_I2CM_STATUS_BUSSTATE(0x01); // Idle.
		sercom->I2CM.BAUD.reg = SERCOM_I2CM_BAUD_BAUD(baud);

		sercom->I2CM.CTRLA.reg |= SERCOM_I2CM_CTRLA_ENABLE;
		while (sercom->I2CM.SYNCBUSY.reg & SERCOM_I2CM_SYNCBUSY_ENABLE);

		sercom->I2CM.INTENSET.reg =
			// SERCOM_I2CM_INTENSET_SB |
			SERCOM_I2CM_INTENSET_MB |
			SERCOM_I2CM_INTENSET_ERROR;

		sercom_register_handler(sercom_index, twi_master_isr, dev);
		sercom_enable_irq(sercom_index);
	}

	return dev;
}

void twi_master_write(twi_master_t *dev, uint8_t addr, uint8_t len, uint8_t *buf) {
	twi_master_write_read(dev, addr, len, buf, 0, NULL);
}

void twi_master_write_read(twi_master_t *dev, uint8_t addr, uint8_t txbytes, uint8_t *txbuf, uint8_t rxbytes, uint8_t *rxbuf) {

	dev->txbuf = txbuf;
	dev->txbytes = txbytes;

	dev->rxbuf = rxbuf;
	dev->rxbytes = rxbytes; dev->bytes = 0; dev->retries = 0;
	dev->addr = addr << 1;

	/**
	 * @TODO check if it is busy here?
	 */
	twi_master_start_txn(dev);
}

static void twi_master_start_txn(const twi_master_t *const dev) {
	dev->sercom->I2CM.ADDR.reg = SERCOM_I2CM_ADDR_ADDR(dev->addr | (dev->txbytes ? 0 : 1));
}

static void twi_master_txn_complete(twi_master_t *dev, int8_t status) {
	dev->txn_complete(dev->ins, status);
}

static void twi_master_write_handler(twi_master_t *dev) {
	Sercom *sercom = dev->sercom;

	if (sercom->I2CM.STATUS.reg & SERCOM_I2CM_STATUS_RXNACK) {
		// TODO: does the busstate need to be manually set?
		sercom->I2CM.STATUS.reg = SERCOM_I2CM_STATUS_BUSSTATE(0x01);
		if (dev->retries++ < 3) {
			dev->bytes = 0;
			twi_master_start_txn(dev);
		} else {
			sercom->I2CM.CTRLB.reg = SERCOM_I2CM_CTRLB_CMD(0x03);
			twi_master_txn_complete(dev, -1);
		}
	} else {
		if (dev->bytes < dev->txbytes) {
			sercom->I2CM.DATA.reg = dev->txbuf[dev->bytes++];
		} else {
			// TODO: does the busstate need to be manually set?
			sercom->I2CM.CTRLB.reg = SERCOM_I2CM_CTRLB_CMD(0x03);
			sercom->I2CM.STATUS.reg = SERCOM_I2CM_STATUS_BUSSTATE(0x01);

			//send repeated start if bytes to read
			if (dev->rxbytes) {
				dev->bytes = 0;
				// must be 0 to write correct direction bit
				dev->txbytes = 0;
				twi_master_start_txn(dev);
			} else {
				twi_master_txn_complete(dev, 0);
			}
		}
	}
}

void twi_master_set_callback(twi_master_t *dev, void *ins, twi_master_complete_txn_cb txn_complete) {
	if (dev) {
		dev->ins = ins;
		dev->txn_complete = txn_complete;
	}
}

static void twi_master_isr(void *_isr_ins) {
	twi_master_t *dev = _isr_ins;
	Sercom *sercom = dev->sercom;

	if (sercom->I2CM.INTFLAG.reg & SERCOM_I2CM_INTFLAG_MB) {
		twi_master_write_handler(dev);
		
	} else if (sercom->I2CM.INTFLAG.reg & SERCOM_I2CM_INTFLAG_ERROR) {
		sercom->I2CM.INTFLAG.reg = SERCOM_I2CM_INTFLAG_ERROR;

		// arbitration lost - if the bus is not idle, issuing another start
		// will cause HW to wait for the bus to become idle before starting the
		// new transaction.
		if (dev->retries++ < 3) {
			if (sercom->I2CM.STATUS.reg & SERCOM_I2CM_STATUS_ARBLOST) {
				dev->bytes = 0;
				twi_master_start_txn(dev);
			} else { // TODO
			}
		}
	}
}
