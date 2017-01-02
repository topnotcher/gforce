#include <stdint.h>

#include "mpc.h"

#ifndef _MPCTWI_H
#define _MPCTWI_H

mpc_driver_t *mpctwi_init(void);
void mpctwi_send(mpc_driver_t *, uint8_t, uint8_t, uint8_t *);

#endif
