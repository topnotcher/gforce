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
#include "display.h"
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
	display_init();

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
//	uint8_t led_pattern[] = {8,0,16,16,16,16,1,6,0,2,2,2,2,1,6,0,48,48,48,48,1,6,0,6,6,6,6,1,6,0,128,128,128,128,1,6,0,15,15,15,15,1,6,0,112,112,112,112,1,6,0,5,5,5,5,1,6,0};
//	uint8_t led_pattern_size = 58;
//	uint8_t led_pattern[] = {1, 1, (COLOR_RED<<4 | COLOR_BLUE) , (COLOR_ORANGE<<4 | COLOR_CYAN) , (COLOR_PINK<<4 | COLOR_GREEN) , (COLOR_PURPLE<<4 | COLOR_YELLOW), 10, 15, 15};	
//	uint8_t led_pattern_size = 9;

	while (1) {
		
/*		 mpc_send(0b0001,'A', led_pattern, led_pattern_size);
		 _delay_ms(2500);
		 mpc_send(0b0010,'A', led_pattern, led_pattern_size);
		 _delay_ms(2500);
		 mpc_send(0b0100,'A', led_pattern, led_pattern_size);
		 _delay_ms(2500);
		 mpc_send(0b1000,'A', led_pattern, led_pattern_size);
		 _delay_ms(2500);
		 phasor_comm_send('A',led_pattern, led_pattern_size);
		_delay_ms(2500);

		 mpc_send_cmd(0b1111,'B');
		 phasor_comm_send('B', NULL, 0);
*/
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
	uint8_t led_pattern[] = {8,0,16,16,16,16,1,6,0,2,2,2,2,1,6,0,48,48,48,48,1,6,0,6,6,6,6,1,6,0,128,128,128,128,1,6,0,15,15,15,15,1,6,0,112,112,112,112,1,6,0,5,5,5,5,1,6,0};
	uint8_t led_pattern_size = 58;


	static uint8_t on = 0;
	if ( pkt == NULL ) return;
	
	if ( (pkt->cmd == 'I' && pkt->data[0] == 0x38 && !on)) {
		on = 1;
		mpc_send(0b1111,'A', led_pattern, led_pattern_size);
		phasor_comm_send('A',led_pattern,led_pattern_size);
		
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


	} else if ( pkt->cmd == 'I' && pkt->data[0] == 0x08 && on )  {
		phasor_comm_send('B',NULL,0);
		mpc_send_cmd(0b1111,'B');
		on = 0;
	}

		//set_lights(1);

	free(pkt);
}
