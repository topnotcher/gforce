#include <twi_master.h>

#ifndef DS2483_H
#define DS2483_H

/**
 * One wire BUS reset/presence detect cycle
 */
#define DS2483_CMD_BUS_RST 0xb4

/**
 * Reset the DS2483 device
 */
#define DS2483_CMD_RST 0xF0

#define DS2483_CMD_SET_READ_PTR 0xE1
#define DS2483_CMD_1W_WRITE_BYTE 0xA5
#define DS2483_CMD_1W_READ_BYTE 0x96

#define DS2483_REGISTER_STATUS 0xF0
#define DS2483_REGISTER_READ_DATA 0xE1
#define DS2483_REGISTER_DEVICE_CONFIG 0xC3
#define DS2483_REGISTER_PORT_CONFIG 0xB4

#define DS2483_STATUS_1WB 0x01
#define DS2483_STATUS_PPD 0x02

#define DS2483_I2C_ADDR 0x18

struct ds2483_dev_struct;
typedef struct ds2483_dev_struct {
	twi_master_t * twim;
	PORT_t * slpz_port;
	uint8_t slpz_pin;
	uint8_t cmd[2];
	volatile uint8_t result;
} ds2483_dev_t;


ds2483_dev_t * ds2483_init(twi_master_t * twim, PORT_t * splz_port, uint8_t splz_pin);

uint8_t ds2483_1w_rst(ds2483_dev_t * dev);
void ds2483_rst(ds2483_dev_t * dev);
uint8_t ds2483_read_register(ds2483_dev_t * dev, uint8_t reg);
uint8_t ds2483_read_byte(ds2483_dev_t * dev);
void ds2483_set_read_ptr(ds2483_dev_t * dev, uint8_t reg);
uint8_t ds2483_1w_read_byte(ds2483_dev_t * dev);
void ds2483_1w_write(ds2483_dev_t * dev, uint8_t data);
#define DS2483_INTERRUPT_HANDLER(ISR, dev) ISR { twi_master_isr(dev->twim); }

#endif
