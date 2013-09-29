#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

//last 4 bits = reserved for "slave" boardz
#define MPC_TWI_ADDR 0x40
#define MPC_TWI TWIC
#define MPC_TWI_MASTER_ISR ISR(TWIC_TWIM_vect)
#define MPC_TWI_SLAVE_ISR ISR(TWIC_TWIS_vect)

#define MPC_QUEUE_SIZE 8

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
 * Heap Size
 */
#define MALLOC_HEAP_SIZE 2048


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




#endif
