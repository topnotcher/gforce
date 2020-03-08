#include <stdint.h>

#include "mpc.h"

#ifndef XBEE_H
#define XBEE_H


void xbee_init(void);
void xbee_send(const uint8_t cmd, const uint8_t size ,uint8_t * data);
void xbee_send_pkt(const mpc_pkt *const spkt);


#endif
