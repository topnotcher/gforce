#include <mpc_common.h>

#ifndef MPC_H
#define MPC_H

#define MPC_TWI_ADDRMASK 0b0001111

#define MPC_RECVQ_MAX 8

void mpc_init(void);
void mpc_recv(void);
#endif
