#include <mpc.h>

#ifndef PHASOR_COMM_H
#define PHASOR_COMM_H

void phasor_comm_init(void);
void phasor_comm_send(const uint8_t cmd, const uint8_t size, uint8_t * data);
mpc_pkt * phasor_comm_recv(void);

void phasor_tx_process(void);



#endif
