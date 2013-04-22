#include "pkt.h"

#ifndef MPC_H
#define MPC_H

void mpc_init(void);

void mpc_send_cmd(const uint8_t addr, const uint8_t cmd);

void mpc_send(const uint8_t addr, const uint8_t cmd, uint8_t * const data, const uint8_t len);

pkt_hdr * mpc_recv(void);
#endif
