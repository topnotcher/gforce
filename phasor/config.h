#ifndef BOARD_CONFIG_H

#define BUZZ_PORT PORTA
#define BUZZ_PIN PIN3_bm


/**
 * Configuration specific to this board as required to work with includes etcetc
 */

#define LED_PORT PORTD
#define LED_SCLK_PIN 7
#define LED_SOUT_PIN 5
#define LED_SS_PIN 4
#define LED_SPI SPID
#define LED_TX_vect SPID_INT_vect

/**************************************
 * IR TX configuration
 */

/** IR TX USART **/
#define IRTX_USART USARTC1

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

#define MPC_ADDR_BOARD MPC_ADDR_PHASOR
#define MPC_QUEUE_SIZE 4

#endif
