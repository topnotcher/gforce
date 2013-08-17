#include <stdint.h>
#include <mpc.h>

#ifndef DISPLAY_H
#define DISPLAY_H

void comm_init(void);
mpc_pkt * comm_recv(void);

#endif
