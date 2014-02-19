#include <avr/io.h>

#ifndef TWI_SLAVE_H
#define TWI_SLAVE_H

#define twi_slave_direction(twi) (((twi)->STATUS & TWI_SLAVE_DIR_bm) ? 1 : 0)

typedef struct {
	TWI_SLAVE_t * twi;
	uint8_t addr;
	void * ins;

	uint8_t * buf;
	uint8_t buf_size;
	uint8_t bytes;

	void (*begin_txn)(void *ins, uint8_t write, uint8_t ** buf, uint8_t * buf_size);
	void (*end_txn)(void * ins, uint8_t write, uint8_t * buf, uint8_t buf_size);
} twi_slave_t;

twi_slave_t * twi_slave_init(
		TWI_SLAVE_t * twi, 
		uint8_t addr,
		uint8_t mask,
		void * ins,
		void (*begin_txn)(void *ins, uint8_t write, uint8_t ** buf, uint8_t * buf_size),
		void (*end_txn)(void * ins, uint8_t write, uint8_t * buf, uint8_t buf_size)
);


void twi_slave_isr(twi_slave_t * dev); 
#endif
