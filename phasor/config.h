#ifndef BOARD_CONFIG_H

#define MPC_TWI_ADDR 16

/**
 * Configuration specific to this board as required to work with includes etcetc
 */

#define LED_PORT PORTD
#define LED_SCLK_PIN 7
#define LED_SOUT_PIN 5
#define LED_SS_PIN 4
#define LED_SPI SPID
#define LED_SPI_vect SPID_INT_vect

/**************************************
 * IR TX configuration
 */

/** IR TX USART **/
#define IRTX_USART USARTC1
#define IRTX_USART_DRE_vect USARTC1_DRE_vect

#define IRTX_USART_PORT PORTC
#define IRTX_USART_TXPIN 7


/** IR TX Timer **/
#define IRTX_TIMER TCD0
#define IRTX_TIMER_PORT PORTD
#define IRTX_TIMER_PIN 0
#define IRTX_TIMER_CCx CCA


/**************************************
 * IR RX configuration
 */
#define IRRX_USART USARTD0
#define IRRX_USART_RXC_vect USARTD0_RXC_vect

#define IRRX_USART_PORT PORTD
#define IRRX_USART_RXPIN 2




/**************************************
 * Trigger configuration
 */
#define TRIGGER_PORT PORTA
#define TRIGGER_PIN 2

/**************************************
 * Master Communication Configuration
 */
#define PHASOR_COMM_USART USARTE0
#define PHASOR_COMM_USART_PORT PORTE
#define PHASOR_COMM_USART_TXPIN 3
#define PHASOR_COMM_USART_RXPIN 2


#define PHASOR_COMM_TXC_ISR ISR(USARTE0_DRE_vect)
#define PHASOR_COMM_RXC_ISR ISR(USARTE0_RXC_vect)

#define MPC_ADDR 0x10
#define MPC_QUEUE_SIZE 4

#define MALLOC_HEAP_SIZE 512

#endif
