#include <stdint.h>
#include <string.h>

#include <malloc.h>
#include <sam.h>

#include <drivers/sam/sercom.h>
#include <drivers/sam/twi/master.h>

struct _twi_master_s {
	sercom_t *sercom;
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
static void twi_master_isr(sercom_t *);

twi_master_t *twi_master_init(Sercom *hw, const uint8_t baud) {
	int sercom_index = sercom_get_index(hw);
	twi_master_t *dev = smalloc(sizeof *dev);

	if (dev) {
		memset(dev, 0, sizeof(*dev));

		dev->sercom = sercom_init(sercom_index);

		sercom_enable_pm(sercom_index);
		sercom_set_gclk_core(sercom_index, GCLK_PCHCTRL_GEN_GCLK0);
		sercom_set_gclk_slow(sercom_index, GCLK_PCHCTRL_GEN_GCLK0);

		// Disable module
		// Parts of CTRLA and CTRLB are enabled-protected
		hw->I2CM.CTRLA.reg &= ~SERCOM_I2CM_CTRLA_ENABLE;
		while (hw->I2CM.SYNCBUSY.reg & SERCOM_I2CM_SYNCBUSY_ENABLE);

		uint32_t ctrla = SERCOM_I2CM_CTRLA_MODE(0x05);
		uint32_t ctrlb = 0;

		// set MODE
		hw->I2CM.CTRLA.reg = ctrla;

		hw->I2CM.CTRLA.reg = ctrla;
		hw->I2CM.CTRLB.reg = ctrlb;
		hw->I2CM.BAUD.reg = SERCOM_I2CM_BAUD_BAUD(baud);

		hw->I2CM.CTRLA.reg |= SERCOM_I2CM_CTRLA_ENABLE;
		while (hw->I2CM.SYNCBUSY.reg & (SERCOM_I2CM_SYNCBUSY_ENABLE | SERCOM_I2CM_SYNCBUSY_SYSOP));

		hw->I2CM.STATUS.reg = SERCOM_I2CM_STATUS_BUSSTATE(0x01u); // Idle.
		while (hw->I2CM.SYNCBUSY.reg & (SERCOM_I2CM_SYNCBUSY_ENABLE | SERCOM_I2CM_SYNCBUSY_SYSOP));

		hw->I2CM.INTENSET.reg =
			// SERCOM_I2CM_INTENSET_SB |
			SERCOM_I2CM_INTENSET_MB |
			SERCOM_I2CM_INTENSET_ERROR;

		sercom_register_handler(dev->sercom, twi_master_isr);
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
	dev->sercom->hw->I2CM.ADDR.reg = SERCOM_I2CM_ADDR_ADDR(dev->addr | (dev->txbytes ? 0 : 1));
}

static void twi_master_txn_complete(twi_master_t *dev, int8_t status) {
	dev->txn_complete(dev->ins, status);
}

static void twi_master_write_handler(twi_master_t *dev) {
	Sercom *hw = dev->sercom->hw;

	if (hw->I2CM.STATUS.reg & SERCOM_I2CM_STATUS_RXNACK) {
		// TODO: does the busstate need to be manually set?
		hw->I2CM.STATUS.reg = SERCOM_I2CM_STATUS_BUSSTATE(0x01);
		if (dev->retries++ < 3) {
			dev->bytes = 0;
			twi_master_start_txn(dev);
		} else {
			hw->I2CM.CTRLB.reg = SERCOM_I2CM_CTRLB_CMD(0x03);
			twi_master_txn_complete(dev, -1);
		}
	} else {
		if (dev->bytes < dev->txbytes) {
			hw->I2CM.DATA.reg = dev->txbuf[dev->bytes++];
		} else {
			// TODO: does the busstate need to be manually set?
			hw->I2CM.CTRLB.reg = SERCOM_I2CM_CTRLB_CMD(0x03);
			hw->I2CM.STATUS.reg = SERCOM_I2CM_STATUS_BUSSTATE(0x01);

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

static void twi_master_isr(sercom_t *s) {
	twi_master_t *dev = from_sercom(dev, s, sercom);
	Sercom *hw = s->hw;

	if (hw->I2CM.INTFLAG.reg & SERCOM_I2CM_INTFLAG_MB) {
		twi_master_write_handler(dev);
		
	} else if (hw->I2CM.INTFLAG.reg & SERCOM_I2CM_INTFLAG_ERROR) {
		hw->I2CM.INTFLAG.reg = SERCOM_I2CM_INTFLAG_ERROR;

		// arbitration lost - if the bus is not idle, issuing another start
		// will cause HW to wait for the bus to become idle before starting the
		// new transaction.
		if (dev->retries++ < 3) {
			if (hw->I2CM.STATUS.reg & SERCOM_I2CM_STATUS_ARBLOST) {
				dev->bytes = 0;
				twi_master_start_txn(dev);
			} else { // TODO
			}
		}
	}
}
