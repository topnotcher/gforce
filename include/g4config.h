#ifndef G4_CONFIG_H
#define G4_CONFIG_H

/* BAUD: 32000000/(2*400000) - 5 = 35*/
#define MPC_TWI_BAUD 35
#define MPC_CRC_POLY 0x32
#define MPC_CRC_SHIFT 0x67

#define IR_USART_BSEL 164
#define IR_USART_BSCALE 3

#endif
