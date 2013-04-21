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

#define _txc_interrupt_enable() IRTX_USART.CTRLA |= USART_DREINTLVL_MED_gc
#define _txc_interrupt_disable() IRTX_USART.CTRLA &= ~USART_DREINTLVL_MED_gc


#define TX_QUEUE_MAX 25

typedef struct {
	uint8_t read;
	uint8_t write;
	uint16_t data[TX_QUEUE_MAX];
} queue_t;

queue_t sendq;


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
	//stop bits mode 1 => 2 stop bits for v4 compat
	IRTX_USART.CTRLC = USART_PMODE_DISABLED_gc | USART_CHSIZE_9BIT_gc | (1<<USART_SBMODE_bp) ;

	//assuming 32MHZ CPU clk per
	TCD0.CTRLA = TC_CLKSEL_DIV2_gc;
	TCD0.CTRLB = TC_WGMODE_FRQ_gc;
	// 32000000/(2*2(210+1)) (XMEGAA, PAGE 160).
	PORTD.DIRSET = PIN0_bm;
	TCD0.CCA = 210;

	sendq.read = sendq.write = 0;

}

void irtx_send(uint16_t data[], uint8_t size) {
	for ( uint8_t i = 0; i < size; ++i )
		irtx_put(data[i]);
}

void irtx_put(uint16_t data) {

	sendq.data[sendq.write] = data;
	sendq.write = (sendq.write == TX_QUEUE_MAX-1) ? 0 : sendq.write+1;
	
	_txc_interrupt_enable();
}


//inline void irtx_byte(uint8_t byte) {
//	IRTX_USART.CTRLB USART_TXB8_bp
//	IRTX_USART.DATA = byte;
//}

//the logic in this is backwarsd from what I would expect???
ISR(PORTC_INT0_vect) {
	if ( !(IRTX_USART_PORT.IN & _TXPIN_bm) )
		TCD0.CTRLB |= TC0_CCAEN_bm;
	else 
		TCD0.CTRLB &= ~TC0_CCAEN_bm;

}

ISR(USARTC1_DRE_vect) {
	uint16_t data = sendq.data[sendq.read];
	
	if ( data & 0x100 )
		IRTX_USART.CTRLB |= 1<<USART_TXB8_bp;
	else 
		IRTX_USART.CTRLB &= ~(1<<USART_TXB8_bp);


	IRTX_USART.DATA = data;

	sendq.read = (sendq.read == TX_QUEUE_MAX-1) ? 0 : sendq.read+1;

	if ( sendq.read == sendq.write )
		_txc_interrupt_disable();

}
