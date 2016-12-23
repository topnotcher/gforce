#include <mpc.h>
#include <comm.h>
#include <mempool.h>

#ifndef PHASOR_COMM_H
#define PHASOR_COMM_H

comm_driver_t * phasor_comm_init(mempool_t * mempool, uint8_t addr, void (*end_rx)(comm_driver_t*));
void phasor_comm_send(comm_driver_t *comm, uint8_t daddr, const uint8_t size, uint8_t *buf);

#endif
