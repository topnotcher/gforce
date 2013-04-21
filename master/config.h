#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

//last 4 bits = reserved for "slave" boardz
#define MPC_TWI_ADDR 0b1000000
#define MPC_TWI TWIC
#define MPC_TWI_MASTER_ISR ISR(TWIC_TWIM_vect)

#endif
