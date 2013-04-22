#include <stdint.h>
#include <string.h>
#include <util/delay.h>

//#include "lcd.h"
#include "sounds.h"
#include "xbee.h"
#include "game.h"
#include <colors.h>
#include <mpc.h>

volatile xbee_queue_t xbee_sendq;

inline void xbee_init() {

	/**
	 * Note: BRR can also be set via high/low nibbles.
	 * UBRR0H = (uint8_t)((baud >> 8)&0x0F);
     * UBRR0L = (uint8_t)(baud & 0xFF);
	 */
	/*XBEE_UBRR = (uint16_t)( XBEE_UBRR_VALUE & 0x0FFF );

	//BSCALE
	USARTF0.BAUDCTRLB = 4<<USART_BSCALE_gp;
*/

	
	XBEE_BAUDA = (uint8_t)( XBEE_BSEL_VALUE & 0x00FF );
	XBEE_BAUDB = (XBEE_BSCALE_VALUE<<USART_BSCALE_gp) | (uint8_t)( (XBEE_BSEL_VALUE>>8) & 0x0F ) ;

	XBEE_CSRC = XBEE_CSRC_VALUE;
	XBEE_CSRA = XBEE_CSRA_VALUE;	
	XBEE_CSRB = XBEE_CSRB_VALUE;


	//Manual, page 237.
	PORTF.OUTSET = PIN3_bm;
	PORTF.DIRSET = PIN3_bm;

//	PORTF.DIRSET |= PIN5_bm | PIN3_bm | PIN7_bm;

//	PORTF.OUTSET |= PIN3_bm;


	memset((void *)&xbee_sendq, 0, sizeof xbee_sendq);
}

void xbee_send(uint8_t data[], uint8_t size) {
	
	for ( uint8_t i = 0; i < size; ++i ) 
		xbee_put(data[i]);
	}

void xbee_put(uint8_t data) {

	xbee_sendq.data[xbee_sendq.write] = data;
	xbee_sendq.write = (xbee_sendq.write == XBEE_QUEUE_MAX-1) ? 0 : xbee_sendq.write+1;
	
	xbee_txc_interrupt_enable();
}

XBEE_TXC_ISR {
	XBEE_UDR = xbee_sendq.data[xbee_sendq.read];

	xbee_sendq.read = (xbee_sendq.read == XBEE_QUEUE_MAX-1) ? 0 : xbee_sendq.read+1;

	if ( xbee_sendq.read == xbee_sendq.write )
		xbee_txc_interrupt_disable();
}

XBEE_RXC_ISR {
	volatile uint8_t meh;
	
	//MUST read the data to clear the interrupt bit or something
	meh = XBEE_UDR;
	//if ( meh == '!' )
	//	lcd_clear();
	//
	 if ( meh == 'F' ) 
		sound_play_effect(SOUND_LASER);
	else if ( meh == 'P' ) 
		sound_play_effect(SOUND_POWER_UP);
	else if ( meh == 'A' ) {

		 uint8_t data[] = {1, 1, (COLOR_RED<<4 | COLOR_BLUE) , (COLOR_ORANGE<<4 | COLOR_CYAN) , (COLOR_PINK<<4 | COLOR_GREEN) , (COLOR_PURPLE<<4 | COLOR_YELLOW), 10, 15, 15};	
		 mpc_send(0b1111,'A', data, 9);

	} else if ( meh == 'B' ) {
		mpc_send_cmd(0b1111, 'B');
	} else if ( meh == 'C' ) {
		mpc_send_cmd(0b1111, 'C');
	} else if ( meh == 'D' ) {
		mpc_send_cmd(0b1111, 'D');
	}
/*	else if ( meh != 10 )
		lcd_write(meh);*/
}
