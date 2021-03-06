#ifndef BOARD_CONFIG_H

/**
 * Configuration specific to this board as required to work with includes etcetc
 */

#define MPC_TWI_ADDR_EEPROM_ADDR 0
#define MPC_TWI_ADDRMASK 0x3F

#define MPC_ADDR_BOARD (MPC_ADDR_LS | MPC_ADDR_RS)

#define MPC_TWI TWIC

#define BUZZ_PORT PORTA
#define BUZZ_PIN PIN3_bm

/**************************************
 * IR RX configuration
 */
#define IRRX_USART USARTE0
#define IRRX_USART_RXC_vect USARTE0_RXC_vect

#define IRRX_USART_PORT PORTE
#define IRRX_USART_RXPIN 2

#define MPC_QUEUE_SIZE 4

#endif
