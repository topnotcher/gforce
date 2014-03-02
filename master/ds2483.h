#include <twi_master.h>

#ifndef DS2483_H
#define DS2483_H

#define DS2483_CMD_RST 0xb4

#define DS2483_I2C_ADDR 0x18


typedef struct {
	twi_master_t * twim;
	PORT_t * slpz_port;
	uint8_t slpz_pin;
} ds2483_dev_t;


ds2483_dev_t * ds2483_init(TWI_MASTER_t * twim, uint8_t baud, PORT_t * splz_port, uint8_t splz_pin); 

void ds2483_bus_rst(ds2483_dev_t * dev); 
#endif
