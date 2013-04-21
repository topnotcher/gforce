#include <avr/io.h>
#include <avr/interrupt.h>

#include "config.h"
#include "g4config.h"

#include <util.h>
#include <leds.h>

#include "irtx.h"

/**
 * Magic macros to minimize my configuration.
 */
#define _TXPIN_bm G4_PIN(IRTX_USART_TXPIN)
#define _TXPINCTRL G4_PINCTRL(IRTX_USART_TXPIN)
#define _TIMERPIN G4_PIN(IRTX_TIMER_PIN)

#define _CCXEN(cc) G4_CONCAT3(TC0_,cc,EN_bm)
#define _CCXEN_bm _CCXEN(IRTX_TIMER_CCx)

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
	
	/** These values are from G4CONFIG, NOT board config **/
	IRTX_USART.BAUDCTRLA = (uint8_t)(IR_USART_BSEL&0x00FF);
	IRTX_USART.BAUDCTRLB = (IR_USART_BSCALE<<USART_BSCALE_gp) | (uint8_t)( (IR_USART_BSEL>>8) & 0x0F );

	IRTX_USART.CTRLA = 0;
	IRTX_USART.CTRLB = USART_TXEN_bm;
	//stop bits mode 1 => 2 stop bits for v4 compat
	IRTX_USART.CTRLC = USART_PMODE_DISABLED_gc | USART_CHSIZE_9BIT_gc | (1<<USART_SBMODE_bp) ;

	//assuming 32MHZ CPU clk per
	IRTX_TIMER.CTRLA = TC_CLKSEL_DIV2_gc;
	IRTX_TIMER.CTRLB = TC_WGMODE_FRQ_gc;
	// 32000000/(2*2(210+1)) (XMEGAA, PAGE 160).
	IRTX_TIMER_PORT.DIRSET = _TIMERPIN;
	IRTX_TIMER_PORT.OUTCLR = _TIMERPIN;
	TCD0.IRTX_TIMER_CCx = 210;

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

//the logic in this is backwarsd from what I would expect???
ISR(PORTC_INT0_vect) {
	if ( !(IRTX_USART_PORT.IN & _TXPIN_bm) )
		IRTX_TIMER.CTRLB |= _CCXEN_bm;
	else 
		IRTX_TIMER.CTRLB &= ~_CCXEN_bm;

}

ISR(IRTX_USART_DRE_vect) {
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