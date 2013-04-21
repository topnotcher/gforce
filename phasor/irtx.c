#include <avr/io.h>

#include "config.h"
#include "irtx.h"

inline void irtx_init() {
	
	IRTX_USART_PORT.OUTSET = IRTX_USART_TXPIN;
	IRTX_USART_PORT.DIRSET = IRTX_USART_TXPIN;


	IRTX_USART.BAUDCTRLA = (uint8_t)(IRTX_USART_BSEL&0x00FF);
	IRTX_USART.BAUDCTRLB = (IRTX_USART_BSCALE<<USART_BSCALE_gp) | (uint8_t)( (IRTX_USART_BSEL>>8) & 0x0F );

	IRTX_USART.CTRLA = 0;
	IRTX_USART.CTRLB = USART_TXEN_bm;
	IRTX_USART.CTRLC = USART_PMODE_DISABLED_gc | USART_CHSIZE_8BIT_gc;
}

inline void irtx_byte(uint8_t byte) {
	IRTX_USART.DATA = byte;
}
