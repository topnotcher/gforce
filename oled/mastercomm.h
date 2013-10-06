#include <stdint.h>
#include <mpc.h>

#ifndef DISPLAY_H
#define DISPLAY_H

void mastercomm_init(void);
mpc_pkt * mastercomm_recv(void);

#endif
