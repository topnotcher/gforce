#include <stdint.h>

#include <saml21/io.h>

#ifndef TWI_MASTER_H
#define TWI_MASTER_H

typedef struct {
	Sercom *sercom;
	void (*txn_complete)(void * ins, int8_t status);
	void *ins;
	uint8_t *txbuf;
	uint8_t *rxbuf;

	uint8_t rxbytes;
	uint8_t txbytes;

	uint8_t bytes;
	uint8_t retries;

	// currently addressed slave
	uint8_t addr;
} twi_master_t;


twi_master_t *twi_master_init(
	Sercom *sercom, 
	const uint8_t baud, 
	void *ins,
	void (* txn_complete)(void *, int8_t)
);

void twi_master_write_read(twi_master_t *dev, uint8_t addr, uint8_t txbytes, uint8_t *txbuf, uint8_t rxbytes, uint8_t *rxbuf);
void twi_master_write(twi_master_t *dev, uint8_t addr, uint8_t len, uint8_t *buf); 
//void twi_master_read(twi_master_t * dev, uint8_t addr, uint8_t len, uint8_t * buf); 
//void twi_master_set_blocking(twi_master_t * twim, void (*block)(void), void (*resume)(void));

#endif
