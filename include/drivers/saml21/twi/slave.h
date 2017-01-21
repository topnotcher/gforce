#include <stdint.h>

#include <saml21/io.h>
#include <drivers/twi/slave.h>

#ifndef SAML21_TWI_SLAVE_H
#define SAML21_TWI_SLAVE_H

twi_slave_t *twi_slave_init(Sercom *, uint8_t, uint8_t);

#endif
