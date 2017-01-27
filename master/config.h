#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

/**
 * MPC TWI Configuration
 */
#define MPC_TWI TWIC

/**
 * MPC (general) configuration
 */
#define MPC_ADDR_BOARD MPC_ADDR_MASTER
#define MPC_QUEUE_SIZE 12

/**************************************
 * Phasor Communication Configuration
 */
#define PHASOR_COMM_USART USARTE0
#define PHASOR_COMM_USART_PORT PORTE
#define PHASOR_COMM_USART_TXPIN 3
#define PHASOR_COMM_USART_RXPIN 2

#define PHASOR_COMM_TXC_ISR ISR(USARTE0_DRE_vect)
#define PHASOR_COMM_RXC_ISR ISR(USARTE0_RXC_vect)

/**************************************
 * XBee Configuration
 */
#define XBEE_BSEL_VALUE 12
#define XBEE_BSCALE_VALUE 4

#define XBEE_PORT PORTF
#define XBEE_TX_PIN 3

#define XBEE_USART USARTF0

/**
 * ISR for transmitting
 */
#define XBEE_TXC_ISR ISR(USARTF0_DRE_vect)

/**
 * ISR for received bytes
 */
#define XBEE_RXC_ISR ISR(USARTF0_RXC_vect)


#define XBEE_QUEUE_MAX 25

#define MPC_PROCESS_XBEE 1



/**
 * ibutton
 */
#define DS2483_SLPZ_PORT PORTD
#define DS2483_SLPZ_PIN 7
#define DS2483_TWI TWIE



#endif
