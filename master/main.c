#include <stdlib.h>
#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/wdt.h>

#include <scheduler.h>
//#include "lcd.h"
#include "lights.h"
//#include "ir_sensor.h"
#include "sounds.h"
//#include "game.h"
#include "xbee.h"
#include "display.h"
#include <phasor_comm.h>
#include <mpc.h>
#include <colors.h>
//#include <leds.h>
#include "game.h"

#ifndef DEBUG
#define DEBUG 1
#endif


#define CLKSYS_Enable( _oscSel ) ( OSC.CTRL |= (_oscSel) )
#define CLKSYS_IsReady( _oscSel ) ( OSC.STATUS & (_oscSel) )

static inline void process_ib_pkt(mpc_pkt const * const pkt);


int main(void) {
	/* Enable 32 MHz Osc. */ 
	CLKSYS_Enable( OSC_RC32MEN_bm ); 
	/* Wait for 32 MHz Osc. to stabilize */
	while ( CLKSYS_IsReady( OSC_RC32MRDY_bm ) == 0 ); 
	// was 8  PLL mult. factor ( 2MHz x8 ) and set clk source to PLL. 
	OSC_PLLCTRL = 16;  

	//enable PLL
	OSC_CTRL = 0x10;

	//wait for PLL clk to be ready
	while( !( OSC_STATUS & 0x10 ) );

	//Unlock seq. to access CLK_CTRL
	CCP = 0xD8; 

	// Select PLL as sys. clk. These 2 lines can ONLY go here to engage the PLL ( reverse of what manual A pg 81 says )
	CLK_CTRL = 0x04;

	scheduler_init();
	sound_init();
	xbee_init();
	phasor_comm_init();
	mpc_init();
	display_init();

	//clear shit by default.
	lights_off();

	display_write("Good Morning");
	_delay_ms(500);

	PMIC.CTRL |= PMIC_MEDLVLEN_bm | PMIC_LOLVLEN_bm | PMIC_HILVLEN_bm;
	sei();

	_delay_ms(500);
	display_write("");
	wdt_enable(9);

	while (1) {
		wdt_reset();
		process_ib_pkt(xbee_recv());
		process_ib_pkt(mpc_recv());
		process_ib_pkt(phasor_comm_recv());

	}

	return 0;
}


static inline void process_ib_pkt(mpc_pkt const * const pkt) {

	if ( pkt == NULL ) return;
	
	if ( pkt->cmd == 'I' )
		process_ir_pkt(pkt);
	else if ( pkt->cmd == 'T' ) {
		uint8_t data[] = {16,3,255, 56, 127 ,138,103,83,0,15,15,68,72,0,44,1,88,113};
		phasor_comm_send('T', data, 18);
	}

}
