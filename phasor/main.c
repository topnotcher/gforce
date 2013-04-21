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
#include "irtx.h"
#include "trigger.h"

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

	PMIC.CTRL |= PMIC_MEDLVLEN_bm;


	//led_config_t led_config = { &PORTD, PIN5_bm, PIN7_bm };

/*	led_config.port = &PORTD;
	led_config.sout = PIN5_bm;
	led_config.sclk = PIN7_bm;*/

	//safe to pass PTR because we never leave main()
	led_init();
	irtx_init();
	trigger_init();
//	PORTC.DIRSET |= 0xff;

//	sei();
	set_lights(1);


//	PORTA.DIRSET |= 0xFF;
//

	uint16_t data[] = {(1<<8) | 255,(1<<8) | 56, (1<<8), (1<<8) | 127,138,103,83,0,15,15,68,72,0,44,1,88,113};

	while(1) {
//		USARTC1.DATA = 'A';
		
		_delay_ms(1000);
//		if ( trigger_pressed() ) 
			irtx_send(data,16);


		leds_run();
	}

	return 0;
}
