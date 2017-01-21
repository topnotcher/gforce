#include <stdint.h>

#include <drivers/twi/master.h>
#include <drivers/twi/slave.h>

#include "mpc.h"

#ifndef _MPCTWI_H
#define _MPCTWI_H

mpc_driver_t *mpctwi_init(twi_master_t *, twi_slave_t *, uint8_t, uint8_t);
void mpctwi_send(mpc_driver_t *, uint8_t, uint8_t, uint8_t *);

#endif
