#include <stdlib.h>
#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>


#include "scheduler.h"
//#include "lcd.h"
//#include "lights.h"
//#include "ir_sensor.h"
#include "sounds.h"
//#include "game.h"
#include "xbee.h"
#include <phasor_comm.h>
#include <mpc.h>
#include <colors.h>
//#include <leds.h>

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
	//lights_init();
//	ir_sensor_init();
//	lcd_init();
	sound_init();
	xbee_init();
	phasor_comm_init();
//	game_init();
	mpc_init();

	PMIC.CTRL |= PMIC_MEDLVLEN_bm | PMIC_LOLVLEN_bm | PMIC_HILVLEN_bm;
	sei();

	#if DEBUG == 0
	/** 
	 * Setting all three bits is extended standby.
	 * Setting no bits is idle. Dicks get sucked if you try to
	 * use USART in extended standby. (no wakey wakey!!)
	 */
//	SMCR = /*_BV(SM1) |  _BV(SM2) | _BV(SM0) |*/ _BV(SE);
	#endif

	while (1) {
		
		//mpc_pkt * pkt = xbee_recv();//mpc_recv();
		process_ib_pkt(mpc_recv());
		process_ib_pkt(phasor_comm_recv());

		#if DEBUG == 0
//		sleep_cpu();
		#endif
	}

	return 0;
}


static inline void process_ib_pkt(mpc_pkt * pkt) {
	static uint8_t on = 0;
	if ( pkt == NULL ) return;

	if ( (pkt->cmd == 'I' && pkt->data[0] == 0x38 && (on^=1))) {
		 uint8_t data[] = {1, 1, (COLOR_RED<<4 | COLOR_BLUE) , (COLOR_ORANGE<<4 | COLOR_CYAN) , (COLOR_PINK<<4 | COLOR_GREEN) , (COLOR_PURPLE<<4 | COLOR_YELLOW), 10, 15, 15};	
		 mpc_send(0b1111,'A', data, 9);
		 phasor_comm_send('A',data,9);
	} else {
		phasor_comm_send('B',NULL,0);
		mpc_send_cmd(0b1111,'B');
	}

		//set_lights(1);

	free(pkt);
}
