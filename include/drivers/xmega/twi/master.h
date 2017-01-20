#include <avr/io.h>

#include <drivers/twi/master.h>

#ifndef XMEGA_TWI_MASTER_H
#define XMEGA_TWI_MASTER_H

twi_master_t *twi_master_init(
			TWI_MASTER_t * twi, 
			const uint8_t baud, 
			void * ins,
			void (* txn_complete)(void *, int8_t));

void twi_master_isr(twi_master_t *dev);
#endif
