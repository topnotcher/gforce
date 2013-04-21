#ifndef BOARD_CONFIG_H

/**
 * Configuration specific to this board as required to work with includes etcetc
 */

#define LED_PORT PORTC
#define LED_SCLK_PIN 7
#define LED_SOUT_PIN 5

#define MPC_TWI_ADDR 0b0100
#define MPC_TWI_ADDRMASK 0b0001111

#define MPC_TWI TWIC
#define MPC_TWI_SLAVE_ISR ISR(TWIC_TWIS_vect)

#define BUZZ_PORT PORTA
#define BUZZ_PIN PIN3_bm

/**************************************
 * IR RX configuration
 */
#define IRRX_USART USARTE0
#define IRRX_USART_RXC_vect USARTE0_RXC_vect

#define IRRX_USART_PORT PORTE
#define IRRX_USART_RXPIN 2


#endif
