#include <stdint.h>
#include <mpc.h>
#include <displaycomm.h>

#ifndef DISPLAY_H
#define DISPLAY_H

void mastercomm_init(void);
display_pkt * mastercomm_recv(void);

#endif
