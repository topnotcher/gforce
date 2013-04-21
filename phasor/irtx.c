#include <avr/io.h>
#include <avr/interrupt.h>

#include "config.h"

#include <util.h>
#include <leds.h>

#include "irtx.h"

/**
 * Magic macros to minimize my configuration.
 */
#define _PIN(id) G4_CONCAT3(PIN,id,_bm)
#define _PINCTRL(id) G4_CONCAT3(PIN,id,CTRL)
#define _TXPIN_bm _PIN(IRTX_USART_TXPIN)
#define _TXPINCTRL _PINCTRL(IRTX_USART_TXPIN)


inline void irtx_init() {
	
	IRTX_USART_PORT.OUTCLR = _TXPIN_bm;
	IRTX_USART_PORT.DIRSET = _TXPIN_bm;
	
	IRTX_USART_PORT._TXPINCTRL |= PORT_INVEN_bm;

	//configure interrupts
	IRTX_USART_PORT.INT0MASK = _TXPIN_bm;
	IRTX_USART_PORT.INTCTRL = PORT_INT0LVL_MED_gc;
	//end interrupts
	
	IRTX_USART.BAUDCTRLA = (uint8_t)(IRTX_USART_BSEL&0x00FF);
	IRTX_USART.BAUDCTRLB = (IRTX_USART_BSCALE<<USART_BSCALE_gp) | (uint8_t)( (IRTX_USART_BSEL>>8) & 0x0F );

	IRTX_USART.CTRLA = 0;
	IRTX_USART.CTRLB = USART_TXEN_bm;
	IRTX_USART.CTRLC = USART_PMODE_DISABLED_gc | USART_CHSIZE_8BIT_gc;

	//assuming 32MHZ CPU clk per
/*	TCD0.CTRLA = TC_CLKSEL_DIV2_gc;
	TCD0.CTRLB = TC_WGMODE_FRQ_gc;
	// 32000000/(2*2(210+1)) (XMEGAA, PAGE 160).
	PORTD.DIRSET = PIN0_bm;
	TCD0.CCA = 210;*/

}

inline void irtx_byte(uint8_t byte) {
	IRTX_USART.DATA = byte;
}

ISR(PORTD_INT0_vect) {
//	TCD0.CTRLB |= TC0_CCAEN_bm;
}
