#include <stdlib.h>
#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>


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

static inline void process_ib_pkt(mpc_pkt * pkt);


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

	_delay_ms(500);
	display_write("Good Morning");

	PMIC.CTRL |= PMIC_MEDLVLEN_bm | PMIC_LOLVLEN_bm | PMIC_HILVLEN_bm;
	sei();

	_delay_ms(500);
	display_write("");

	while (1) {

		process_ib_pkt(xbee_recv());
		process_ib_pkt(mpc_recv());
		process_ib_pkt(phasor_comm_recv());
	}

	return 0;
}


static inline void process_ib_pkt(mpc_pkt * pkt) {

	if ( pkt == NULL ) return;
	
	if ( (pkt->cmd == 'I' && pkt->data[0] == 0x38)) {
		start_game_cmd((command_t*)pkt->data);	

	} else if ( pkt->cmd == 'I' && pkt->data[0] == 0x08 )  {
		stop_game_cmd((command_t*)pkt->data);
	} else if ( pkt->cmd == 'I' && pkt->data[0] == 0x0c ) {

		char * sensor;
		switch(pkt->saddr) {
		case 2:
			sensor = "CH";
			break;
		case 4:
			sensor = "LS";
			break;
		case 8:
			sensor = "BA";
			break;
		case 16: 
			sensor = "RS";
			break;
		case 32:
			sensor = "PH";
			break;
		default:
			sensor = "??";
			break;
		}

		display_send(0, (uint8_t *)sensor, 3);


		if ( pkt->saddr == 8 || pkt->saddr == 2 )
			do_deac();
		else
			do_stun();
	}

	free(pkt);
}
