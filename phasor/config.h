#ifndef BOARD_CONFIG_H

/**
 * Configuration specific to this board as required to work with includes etcetc
 */

#define LED_PORT PORTD
#define LED_SCLK_PIN 7
#define LED_SOUT_PIN 5


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
 * Trigger configuration
 */
#define TRIGGER_PORT PORTA
#define TRIGGER_PIN 2

#endif
