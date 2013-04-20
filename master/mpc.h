#include <avr/interrupt.h>
#include <mpc_common.h>


#ifndef MPC_H
#define MPC_H

#define MPC_TWI TWIC
#define MPC_TWI_MASTER_ISR ISR(TWIC_TWIM_vect)

void mpc_init(void);


//send a command with 0 data lenght
void mpc_send_cmd(const uint8_t addr, const uint8_t cmd);

//send whatever.
void mpc_send(const uint8_t addr, const uint8_t cmd, uint8_t * const data, const uint8_t len);


//check if there is any data to send. If there is, configure the sender magig
void mpc_run(void);


#endif
