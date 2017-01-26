#include <avr/io.h>

#include <drivers/twi/master.h>

#ifndef XMEGA_TWI_MASTER_H
#define XMEGA_TWI_MASTER_H

twi_master_t *twi_master_init(TWI_t *, const uint8_t);
#endif
