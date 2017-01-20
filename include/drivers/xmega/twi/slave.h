#include <avr/io.h>

#include <drivers/twi/slave.h>

#ifndef XMEGA_TWI_SLAVE_H
#define XMEGA_TWI_SLAVE_H

twi_slave_t * twi_slave_init(
		TWI_SLAVE_t * twi,
		uint8_t addr,
		uint8_t mask,
		void *ins,
		void (*begin_txn)(void *ins, uint8_t write, uint8_t ** buf, uint8_t * buf_size),
		void (*end_txn)(void * ins, uint8_t write, uint8_t * buf, uint8_t buf_size)
);


void twi_slave_isr(twi_slave_t * dev);
#endif
