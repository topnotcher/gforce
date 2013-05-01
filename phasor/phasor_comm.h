#ifndef PHASOR_COMM_H
#define PHASOR_COMM_H

void phasor_comm_init(void);
void phasor_comm_send(const uint8_t cmd, uint8_t * data, const uint8_t size);
mpc_pkt * phasor_comm_recv(void);


#endif
