#include <stdint.h>

#include <sam.h>
#include <drivers/twi/slave.h>

#ifndef SAML21_TWI_SLAVE_H
#define SAML21_TWI_SLAVE_H

twi_slave_t *twi_slave_init(const int sercom_index, uint8_t, uint8_t);

#endif
