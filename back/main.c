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
#include <mpc.h>
#include <leds.h>
#include <buzz.h>
#include <irrx.h>
#include <eventq.h>
#include <scheduler.h>


#include "charger.h"

#define CLKSYS_Enable( _oscSel ) ( OSC.CTRL |= (_oscSel) )

#define CLKSYS_IsReady( _oscSel ) ( OSC.STATUS & (_oscSel) )

static void mpc_reply_ping(const mpc_pkt * const pkt);

extern uint8_t led_sequence_raw[];

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
	mpc_init();
	led_init();
	buzz_init();
	irrx_init(); 
	eventq_init();
	eventq_offer(charger_init);
	mpc_register_cmd('P', mpc_reply_ping);

	while(1) {
		eventq_run();

		//this interface is broken and stupid.
		ir_pkt_t irpkt;
		ir_rx(&irpkt);
		if ( irpkt.size != 0 ) 
				mpc_send(0x40, 'I', irpkt.size, irpkt.data);
	}


	return 0;
}
static void mpc_reply_ping(const mpc_pkt * const pkt) {
	mpc_send_cmd(pkt->saddr, 'R');
}
