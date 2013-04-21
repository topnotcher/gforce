#include <avr/io.h>
#include <avr/interrupt.h>

#include <util.h>
#include "config.h"
#include "g4config.h"
#include <mpc_master.h>
#include <irrx.h>
#include <leds.h>

#define _RXPIN_bm G4_PIN(IRRX_USART_RXPIN)

inline void irrx_init() {
	
	IRRX_USART_PORT.DIRCLR = _RXPIN_bm;

	/** These values are from G4CONFIG, NOT board config **/
	IRRX_USART.BAUDCTRLA = (uint8_t)(IR_USART_BSEL&0x00FF);
	IRRX_USART.BAUDCTRLB = (IR_USART_BSCALE<<USART_BSCALE_gp) | (uint8_t)( (IR_USART_BSEL>>8) & 0x0F );

	IRRX_USART.CTRLA |= USART_RXCINTLVL_MED_gc;
	IRRX_USART.CTRLB |= USART_RXEN_bm;

	//stop bits mode 1 => 2 stop bits for v4 compat
	IRRX_USART.CTRLC = USART_PMODE_DISABLED_gc | USART_CHSIZE_9BIT_gc | (1<<USART_SBMODE_bp) ;
}

ISR(IRRX_USART_RXC_vect) {
	uint8_t data = IRRX_USART.DATA;
	static uint8_t i = 0;

	if ( data == 0x38 ) {
		mpc_master_send_cmd(0b1000000, 'A');
		set_lights(i^=1);
	}
}
