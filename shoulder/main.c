#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <stdlib.h>


/**
 * G4 common includes.
 */
#include <leds.h>
#include <mpc_slave.h>
#include <mpc_master.h>
#include <pkt.h>

#define CLKSYS_Enable( _oscSel ) ( OSC.CTRL |= (_oscSel) )

#define CLKSYS_IsReady( _oscSel ) ( OSC.STATUS & (_oscSel) )

#include "config.h"

int main(void) {

	// Enable 32 MHz Osc. 
	CLKSYS_Enable( OSC_RC32MEN_bm ); 
	// Wait for 32 MHz Osc. to stabilize 
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

	PMIC.CTRL |= PMIC_LOLVLEN_bm | PMIC_MEDLVLEN_bm;
	sei();

	//safe to pass PTR because we never leave main()
	led_init();
	mpc_init();
	buzz_init();
	irrx_init();

//set_lights(1);
	while(1) {

		pkt_hdr * pkt = mpc_recv();

		if ( pkt != NULL ) {
			if ( pkt->cmd == 'A' ) {
				led_set_seq(pkt->data);
				set_lights(1);
				pkt->data = NULL;
			} else if ( pkt->cmd == 'B' ) {
				set_lights(0);
			} 

			if ( pkt->data != NULL ) {
				free(pkt->data);		
				pkt->data = NULL;
			}
			free(pkt);
		}

		leds_run();
	}

	return 0;
}

