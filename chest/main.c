#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"


/**
 * G4 common includes.
 */
#include <leds.h>
#include <mpc.h>
#include <buzz.h>
#include <irrx.h>

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

	PMIC.CTRL |= PMIC_LOLVLEN_bm | PMIC_MEDLVLEN_bm | /*PMIC.CTRL |*/ PMIC_HILVLEN_bm;
	sei();

	//safe to pass PTR because we never leave main()
	scheduler_init();
	led_init();
	mpc_init();
	buzz_init();
	irrx_init();

//set_lights(1);
	while(1) {

		mpc_pkt * pkt = mpc_recv();

		if ( pkt != NULL ) {
			if ( pkt->cmd == 'A' ) {

				led_sequence * seq  = (led_sequence*)malloc(pkt->len);
				memcpy((void*)seq, &pkt->data,pkt->len);
				led_set_seq(seq);
				set_lights(1);
			} else if ( pkt->cmd == 'B' ) {
				set_lights(0);
			}

			free(pkt);
		}

		leds_run();

		ir_pkt_t irpkt;
		ir_rx(&irpkt);

		if ( irpkt.size != 0 ) {
				mpc_send(0x40, 'I', irpkt.data, irpkt.size);
				free( irpkt.data );
		}
	}



	return 0;
}

