#include <avr/io.h>
#include <util.h>
#include <malloc.h>
#include <twi_master.h>
#include "config.h"
#include "ds2483.h"


static inline void ds2483_write_read(ds2483_dev_t * dev, uint8_t txbytes, uint8_t * txbuf, uint8_t rxbytes, uint8_t * rxbuf) ATTR_ALWAYS_INLINE; 
static inline void ds2483_read(ds2483_dev_t * dev, uint8_t len, uint8_t * buf) ATTR_ALWAYS_INLINE;
static inline void ds2483_write(ds2483_dev_t * dev, uint8_t len, uint8_t * buf) ATTR_ALWAYS_INLINE;

static void ds2483_i2c_txn_complete(void *, uint8_t);

static uint8_t ds2483_cmd;

ds2483_dev_t * ds2483_init(TWI_MASTER_t * twim, uint8_t baud, PORT_t * slpz_port, uint8_t slpz_pin) {
	ds2483_dev_t * dev;
	dev = smalloc(sizeof *dev);

	dev->twim = twi_master_init(twim, baud, dev, ds2483_i2c_txn_complete);
	dev->slpz_port = slpz_port;
	dev->slpz_pin = slpz_pin;	
	
	dev->slpz_port->DIRSET = dev->slpz_pin;
	//@TODO on/off
	dev->slpz_port->OUTSET = dev->slpz_pin;

	return dev;
}

static inline void ds2483_write(ds2483_dev_t * dev, uint8_t len, uint8_t * buf) {
	twi_master_write(dev->twim, DS2483_I2C_ADDR, len, buf);
}

static inline void ds2483_read(ds2483_dev_t * dev, uint8_t len, uint8_t * buf) {
	twi_master_read(dev->twim, DS2483_I2C_ADDR, len, buf);
}


static inline void ds2483_write_read(ds2483_dev_t * dev, uint8_t txbytes, uint8_t * txbuf, uint8_t rxbytes, uint8_t * rxbuf) { 
	twi_master_write_read(dev->twim, DS2483_I2C_ADDR, txbytes, txbuf, rxbytes, rxbuf);
}


static void ds2483_i2c_txn_complete(void * dev, uint8_t status) {
}


/**
 * Performs a reset/presence detect
 */
void ds2483_bus_rst(ds2483_dev_t * dev) {
	ds2483_write(dev, 1, &ds2483_cmd);
}
