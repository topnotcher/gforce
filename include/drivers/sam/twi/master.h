#include <stdint.h>

#include <drivers/twi/master.h>
#include <sam.h>

#ifndef SAML21_TWI_MASTER_H
#define SAML21_TWI_MASTER_H 

twi_master_t *twi_master_init(const int sercom_index, const uint8_t baud);
#endif
