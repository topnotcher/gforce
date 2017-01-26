#include <avr/io.h>

#include <drivers/twi/slave.h>

#ifndef XMEGA_TWI_SLAVE_H
#define XMEGA_TWI_SLAVE_H

twi_slave_t *twi_slave_init(TWI_t *twi, uint8_t addr, uint8_t mask);
#endif
