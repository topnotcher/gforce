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


	PMIC.CTRL |= PMIC_MEDLVLEN_bm;
	sei();

//	while(1) {
	//	xbee_put('A');
//		USARTF0.DATA ='A';
//		sound_play_effect(SOUND_POWER_UP);
//		_delay_ms(10);
//	}

/*
	lcd_bl_on();
	lcd_putstring("  Good Morning  ");
	_delay_ms(500);
	lcd_clear();
	lcd_bl_off();
*/
	#if DEBUG == 0
	/** 
	 * Setting all three bits is extended standby.
	 * Setting no bits is idle. Dicks get sucked if you try to
	 * use USART in extended standby. (no wakey wakey!!)
	 */
//	SMCR = /*_BV(SM1) |  _BV(SM2) | _BV(SM0) |*/ _BV(SE);
	#endif



	while (1) {

		//avoid sleeping for 5 seconds because
		//it's likely that shit will happen
		//this doesn't matter...
		_delay_ms(5000);

		#if DEBUG == 0
//		sleep_cpu();
		#endif
	}

	return 0;
}
