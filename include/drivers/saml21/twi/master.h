#include <stdint.h>

#include <drivers/twi/master.h>
#include <saml21/io.h>

#ifndef SAML21_TWI_MASTER_H
#define SAML21_TWI_MASTER_H 

twi_master_t *twi_master_init(
	Sercom *sercom, 
	const uint8_t baud, 
	void *ins,
	void (* txn_complete)(void *, int8_t)
);

#endif
