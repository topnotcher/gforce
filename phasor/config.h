#ifndef BOARD_CONFIG_H

/**
 * Configuration specific to this board as required to work with includes etcetc
 */

#define LED_PORT PORTD
#define LED_SCLK PIN7_bm
#define LED_SOUT PIN5_bm

#define IRTX_USART USARTD0
#define IRTX_USART_BSEL 12
#define IRTX_USART_BSCALE 4

#define IRTX_USART_PORT PORTD
#define IRTX_USART_TXPIN PIN3_bm


#endif
