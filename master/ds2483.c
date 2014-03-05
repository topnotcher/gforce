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


ds2483_dev_t * ds2483_init(TWI_MASTER_t * twim, uint8_t baud, PORT_t * slpz_port, uint8_t slpz_pin, void (*txn_cb)(ds2483_dev_t *,uint8_t)) {
	ds2483_dev_t * dev;
	dev = smalloc(sizeof *dev);

	dev->twim = twi_master_init(twim, baud, dev, ds2483_i2c_txn_complete);
	dev->slpz_port = slpz_port;
	dev->slpz_pin = slpz_pin;	
	dev->txn_cb = txn_cb;

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

void ds2483_1w_read(ds2483_dev_t * dev) {
	dev->cmd[0] = DS2483_CMD_1W_READ_BYTE;
	ds2483_write(dev, 1, dev->cmd); 
}

void ds2483_1w_write(ds2483_dev_t * dev, uint8_t data) {
	dev->cmd[0] = DS2483_CMD_1W_WRITE_BYTE;
	dev->cmd[1] = data;
	ds2483_write(dev, 2, dev->cmd);
}

void ds2483_set_read_ptr(ds2483_dev_t * dev, uint8_t reg) {
	dev->cmd[0] = DS2483_CMD_SET_READ_PTR;
	dev->cmd[1] = reg;
	ds2483_write(dev, 2, dev->cmd);
}

void ds2483_read_byte(ds2483_dev_t * dev) {
	ds2483_read(dev, 1, (uint8_t*)&dev->result);
}

void ds2483_read_register(ds2483_dev_t * dev, uint8_t reg) {
	dev->cmd[0] = DS2483_CMD_SET_READ_PTR;
	dev->cmd[1] = reg;
	ds2483_write_read(dev, 2, dev->cmd, 1, (uint8_t*)&dev->result);
}

uint8_t * ds2483_last_cmd(ds2483_dev_t* dev) {
	return dev->cmd;
}

uint8_t ds2483_last_data(ds2483_dev_t*dev) {
	return dev->result;
}

static void ds2483_i2c_txn_complete(void * ins, uint8_t status) {
	ds2483_dev_t * dev = ins;

	if (dev->txn_cb)
		dev->txn_cb(dev, 0);
}

void ds2483_rst(ds2483_dev_t * dev) {
	dev->cmd[0] = DS2483_CMD_RST;
	ds2483_write(dev,1,&dev->cmd[0]);
}

/**
 * Performs a reset/presence detect
 */
void ds2483_1w_rst(ds2483_dev_t * dev) {
	dev->cmd[0] = DS2483_CMD_BUS_RST;
	ds2483_write(dev, 1, &dev->cmd[0]);
}
