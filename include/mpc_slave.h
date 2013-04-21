#ifndef MPC_SLAVE_H
#define MPC_SLAVE_H

//@TODO HOW IZ WE DO THIS? 
#define MPC_TWI_ADDRMASK 0b0001111

#define MPC_RECVQ_MAX 8

void mpc_slave_init(void);
void mpc_slave_recv(void);
#endif
