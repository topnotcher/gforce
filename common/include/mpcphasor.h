#include "mpc.h"

#ifndef _MPC_PHASOR
#define _MPC_PHASOR


mpc_driver_t *mpc_phasor_init(void);
void mpc_phasor_send(mpc_driver_t *, uint8_t, uint8_t, uint8_t *);

#endif
