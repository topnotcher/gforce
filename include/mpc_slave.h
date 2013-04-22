#include "pkt.h"
#ifndef MPC_SLAVE_H
#define MPC_SLAVE_H

#define MPC_RECVQ_MAX 8

void mpc_slave_init(void);
pkt_hdr * mpc_slave_recv(void);
#endif
