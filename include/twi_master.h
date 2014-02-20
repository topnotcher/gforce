#include <avr/io.h>
#ifndef TWI_MASTER_H
#define TWI_MASTER_H

typedef struct {
	TWI_MASTER_t * twi;
	void (* txn_complete)(void * ins, uint8_t status);

	void * ins;
	uint8_t * txbuf;
	uint8_t txbytes;

	uint8_t * rxbuf;
	uint8_t rxbytes;

	uint8_t bytes;
	
	/*currently addressed slave*/
	uint8_t addr;
} twi_master_t;


twi_master_t * twi_master_init(
			TWI_MASTER_t * twi, 
			const uint8_t baud, 
			void * ins,
			void (* txn_complete)(void *, uint8_t));

void twi_master_isr(twi_master_t * dev);

void twi_master_write_read(twi_master_t * dev, uint8_t addr, uint8_t txbytes, uint8_t * txbuf, uint8_t rxbytes, uint8_t * rxbuf);
void twi_master_write(twi_master_t * dev, uint8_t addr, uint8_t len, uint8_t * buf); 
void twi_master_read(twi_master_t * dev, uint8_t addr, uint8_t len, uint8_t * buf); 
#endif
