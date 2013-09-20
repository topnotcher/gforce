#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <stdlib.h>
#include <string.h>

/**
 * G4 common includes.
 */
#include <leds.h>
#include "irtx.h"
#include <irrx.h>
#include "trigger.h"

#include <mpc.h>
#include <phasor_comm.h>
#include <scheduler.h>

#define CLKSYS_Enable( _oscSel ) ( OSC.CTRL |= (_oscSel) )

#define CLKSYS_IsReady( _oscSel ) ( OSC.STATUS & (_oscSel) )


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

	PMIC.CTRL |= PMIC_MEDLVLEN_bm | PMIC_LOLVLEN_bm | PMIC_HILVLEN_bm;

	
	led_init();
	irtx_init();
	trigger_init();
	irrx_init();
	phasor_comm_init();
	scheduler_init();
	sei();

	uint16_t data[] = { 255, 56, 127 ,138,103,83,0,15,15,68,72,0,44,1,88,113};
	for ( uint8_t i = 0; i < 16; ++i )
		if ( i > 3 )
			data[i] |= 0x100;

	while(1) {
	
		if ( trigger_pressed() ) 
			irtx_send(data,16);


		mpc_pkt * pkt = phasor_comm_recv();

		if ( pkt != NULL ) {
			if ( pkt->cmd == 'A' ) {

				led_set_seq(pkt->data, pkt->len);
				set_lights(1);
			} else if ( pkt->cmd == 'B' ) {
				set_lights(0);
			}

		}


		ir_pkt_t irpkt;
		ir_rx(&irpkt);

		if ( irpkt.size != 0 ) {
				phasor_comm_send('I', irpkt.data, irpkt.size);
		}


		leds_run();
	}

	return 0;
}
