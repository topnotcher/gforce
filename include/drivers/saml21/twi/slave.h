#include <stdint.h>

#include <saml21/io.h>

#ifndef TWI_SLAVE_H
#define TWI_SLAVE_H
typedef struct {
	Sercom *sercom;
	void *ins;

	void (*begin_txn)(void *ins, uint8_t write, uint8_t **buf, uint8_t *buf_size);
	void (*end_txn)(void *ins, uint8_t write, uint8_t *buf, uint8_t buf_size);

	uint8_t *buf;
	uint8_t buf_size;
	uint8_t bytes;
	uint8_t addr;
} twi_slave_t;

twi_slave_t *twi_slave_init(Sercom *);

#endif
