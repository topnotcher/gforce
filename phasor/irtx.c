#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <string.h>

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

#define TX_QUEUE_MAX 5

typedef struct {
	uint8_t read;
	uint8_t write;

	uint8_t byte;
	uint8_t cnt;

	irtx_pkt * pkts[TX_QUEUE_MAX];
} queue_t;

queue_t sendq;


inline void irtx_init(void) {
	
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

	//NOTE : removed 1<<USART_SBMODE_bp Why does /LW use 2 stop bits?
	IRTX_USART.CTRLC = USART_PMODE_DISABLED_gc | USART_CHSIZE_9BIT_gc ;

	//assuming 32MHZ CPU clk per
	IRTX_TIMER.CTRLA = TC_CLKSEL_DIV2_gc;
	IRTX_TIMER.CTRLB = TC_WGMODE_FRQ_gc;
	// 32000000/(2*2(210+1)) (XMEGAA, PAGE 160).
	IRTX_TIMER_PORT.DIRSET = _TIMERPIN;
	IRTX_TIMER_PORT.OUTCLR = _TIMERPIN;
	TCD0.IRTX_TIMER_CCx = 210;

	sendq.read = sendq.write = 0;
	sendq.byte = 0;
	sendq.cnt = 0;
}

void irtx_send(irtx_pkt * pkt) {

	irtx_pkt * npkt;

	npkt = malloc(sizeof(*npkt) + pkt->size);
	memcpy(npkt,pkt,sizeof(*npkt)+pkt->size);


	sendq.pkts[sendq.write] = npkt;
	sendq.write = (sendq.write == TX_QUEUE_MAX-1) ? 0 : sendq.write+1;
	
	_txc_interrupt_enable();
}

ISR(PORTC_INT0_vect) {
	if ( !(IRTX_USART_PORT.IN & _TXPIN_bm) )
		IRTX_TIMER.CTRLB |= _CCXEN_bm;
	else 
		IRTX_TIMER.CTRLB &= ~_CCXEN_bm;

}

ISR(IRTX_USART_DRE_vect) {
	irtx_pkt * pkt = sendq.pkts[sendq.read];
	uint8_t data = pkt->data[sendq.byte];
	
	if ( sendq.byte > 3 )
		IRTX_USART.CTRLB |= 1<<USART_TXB8_bp;
	else 
		IRTX_USART.CTRLB &= ~(1<<USART_TXB8_bp);


	IRTX_USART.DATA = data;

	//last byte
	if ( ++sendq.byte == pkt->size ) {
		sendq.byte = 0;
		//last retransmission
		if ( ++sendq.cnt == pkt->repeat ) {
			sendq.cnt = 0;
			free(pkt);
			sendq.read = (sendq.read == TX_QUEUE_MAX-1) ? 0 : sendq.read+1;


			if ( sendq.read == sendq.write )
				_txc_interrupt_disable();
		}
	}
}
