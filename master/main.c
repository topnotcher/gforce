#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>


#include "scheduler.h"
//#include "lcd.h"
#include "lights.h"
//#include "ir_sensor.h"
#include "sounds.h"
#include "game.h"
#include "xbee.h"
#include <mpc.h>
#include <colors.h>

#ifndef DEBUG
#define DEBUG 1
#endif


#define CLKSYS_Enable( _oscSel ) ( OSC.CTRL |= (_oscSel) )
#define CLKSYS_IsReady( _oscSel ) ( OSC.STATUS & (_oscSel) )


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
	lights_init();
//	ir_sensor_init();
//	lcd_init();
	sound_init();
	xbee_init();
	game_init();
	mpc_init();

	PMIC.CTRL |= PMIC_MEDLVLEN_bm | PMIC_LOLVLEN_bm | PMIC_HILVLEN_bm;
	sei();
	xbee_put('*');


	#if DEBUG == 0
	/** 
	 * Setting all three bits is extended standby.
	 * Setting no bits is idle. Dicks get sucked if you try to
	 * use USART in extended standby. (no wakey wakey!!)
	 */
//	SMCR = /*_BV(SM1) |  _BV(SM2) | _BV(SM0) |*/ _BV(SE);
	#endif

		 uint8_t data[] = {1, 1, (COLOR_RED<<4 | COLOR_BLUE) , (COLOR_ORANGE<<4 | COLOR_CYAN) , (COLOR_PINK<<4 | COLOR_GREEN) , (COLOR_PURPLE<<4 | COLOR_YELLOW), 10, 15, 15};	
	while (1) {
		
		mpc_pkt * pkt = mpc_recv();

		if ( pkt != NULL ) {

			xbee_put(':');
			xbee_put(pkt->cmd);
			xbee_put(',');
			char src;

			switch(pkt->saddr>>1) {
			case 0b0001:
					src = 'F';
					break;
			case 0b0010:
					src = 'L';
					break;
			case 0b0100:
					src = 'B';
					break;
			case 0b1000:
					src = 'R';
					break;
			default:
					src = '?';
			}

			xbee_put(src);
			xbee_put('\n');

			free(pkt);
		}
	
		_delay_ms(1000);
		mpc_send(0b1111,'A', data, 9);

//		PORTC.OUTCLR = 0xff;
		#if DEBUG == 0
//		sleep_cpu();
		#endif
	}

	return 0;
}

