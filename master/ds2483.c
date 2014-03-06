#include <avr/io.h>
#include <util.h>
#include <malloc.h>
#include <twi_master.h>
#include "config.h"
#include "ds2483.h"

#ifndef DS2483_1W_WAIT_TIMEOUT
	#define DS2483_1W_WAIT_TIMEOUT 10
#endif

static inline void ds2483_write_read(ds2483_dev_t * dev, uint8_t txbytes, uint8_t * txbuf, uint8_t rxbytes, uint8_t * rxbuf) ATTR_ALWAYS_INLINE;
static inline void ds2483_read(ds2483_dev_t * dev, uint8_t len, uint8_t * buf) ATTR_ALWAYS_INLINE;
static inline void ds2483_write(ds2483_dev_t * dev, uint8_t len, uint8_t * buf) ATTR_ALWAYS_INLINE;
static uint8_t ds2483_1w_wait_idle(ds2483_dev_t * dev);

ds2483_dev_t * ds2483_init(twi_master_t * twim, PORT_t * slpz_port, uint8_t slpz_pin) {
	ds2483_dev_t * dev;
	dev = smalloc(sizeof *dev);

	dev->twim = twim;
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

/**
 * Initiates reads on the one wire bus, and reads a single byte,
 * which is stored in the read data register. The read data register
 * is then fetched and returned to the caller.
 *
 * @return a byte read from the one wire bus
 */
uint8_t ds2483_1w_read_byte(ds2483_dev_t * dev) {
	dev->cmd[0] = DS2483_CMD_1W_READ_BYTE;
	ds2483_write(dev, 1, dev->cmd);
	ds2483_1w_wait_idle(dev);
	return ds2483_read_register(dev, DS2483_REGISTER_READ_DATA);
}

/*
 * Writes a byte to the one wire bus.
 */
void ds2483_1w_write(ds2483_dev_t * dev, uint8_t data) {
	dev->cmd[0] = DS2483_CMD_1W_WRITE_BYTE;
	dev->cmd[1] = data;
	ds2483_write(dev, 2, dev->cmd);
	ds2483_1w_wait_idle(dev);
}

/**
 * Sets the read pointer on the DS2483. Subsequent read attempts
 * to the device will read from the specified register.
 */
void ds2483_set_read_ptr(ds2483_dev_t * dev, uint8_t reg) {
	dev->cmd[0] = DS2483_CMD_SET_READ_PTR;
	dev->cmd[1] = reg;
	ds2483_write(dev, 2, dev->cmd);
}

/**
 * Reads a single byte from the DS2483, read from the register
 * set by ds2483_set_read_ptr
 *
 * @return the byte read
 */
uint8_t ds2483_read_byte(ds2483_dev_t * dev) {
	ds2483_read(dev, 1, (uint8_t*)&dev->result);
	return dev->result;
}

/**
 * Sets the read pointer and reads a single byte in one transaction.
 *
 * @return the value of the requested register
 */
uint8_t ds2483_read_register(ds2483_dev_t * dev, uint8_t reg) {
	dev->cmd[0] = DS2483_CMD_SET_READ_PTR;
	dev->cmd[1] = reg;
	ds2483_write_read(dev, 2, dev->cmd, 1, (uint8_t*)&dev->result);
	return dev->result;
}

/**
 * Resets the DS2483
 */
void ds2483_rst(ds2483_dev_t * dev) {
	dev->cmd[0] = DS2483_CMD_RST;
	ds2483_write(dev,1,&dev->cmd[0]);
}

/**
 * Performs a reset/presence detect
 *
 * @return 1 if device present, 0 otherwise
 */
uint8_t ds2483_1w_rst(ds2483_dev_t * dev) {
	dev->cmd[0] = DS2483_CMD_BUS_RST;
	ds2483_write(dev, 1, &dev->cmd[0]);
	return (ds2483_1w_wait_idle(dev) & DS2483_STATUS_PPD) ? 1 : 0;
}

/**
 * Waits for any ongoing one wire bus activity to finish.
 *
 * @return The contents of the status register
 */
static uint8_t ds2483_1w_wait_idle(ds2483_dev_t * dev) {
	uint8_t tries = 0;

	uint8_t result = ds2483_read_register(dev, DS2483_REGISTER_STATUS);
	while (tries++ < DS2483_1W_WAIT_TIMEOUT && (result & DS2483_STATUS_1WB)) {
		/* first call set the read pointer: now we can just issue the read*/
		result = ds2483_read_byte(dev);

		/**
		 * @TODO I can't explain this: Shit doesn't work without a nop here
		 */
		asm volatile ( "nop");
		asm volatile ( "nop");
		asm volatile ( "nop");
		asm volatile ( "nop");
		asm volatile ( "nop");
	}

	return result;
}
