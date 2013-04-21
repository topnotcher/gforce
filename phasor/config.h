#ifndef BOARD_CONFIG_H

/**
 * Configuration specific to this board as required to work with includes etcetc
 */

#define LED_PORT PORTD
#define LED_SCLK PIN7_bm
#define LED_SOUT PIN5_bm


/**************************************
 * IR TX configuration
 */

/** IR TX USART **/
#define IRTX_USART USARTC1
#define IRTX_USART_BSEL 164
#define IRTX_USART_BSCALE 3

#define IRTX_USART_PORT PORTC
#define IRTX_USART_TXPIN 7



/**************************************
 * Trigger configuration
 */
#define TRIGGER_PORT PORTA
#define TRIGGER_PIN PIN2_bm

#endif
