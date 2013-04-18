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

	PMIC.CTRL |= PMIC_LOLVLEN_bm;
	sei();

/*	led_config.port = &PORTD;
	led_config.sout = PIN5_bm;
	led_config.sclk = PIN7_bm;*/

	//safe to pass PTR because we never leave main()
	led_init();
//	PORTC.DIRSET |= 0xff;

//	sei();
	//set_lights(1);

	PORTA.DIRSET |= 0xFF;
/**xmegaA p219**/
	TWIC.SLAVE.CTRLA = TWI_SLAVE_INTLVL_LO_gc | TWI_SLAVE_ENABLE_bm |  TWI_SLAVE_DIEN_bm | TWI_SLAVE_APIEN_bm /*| TWI_SLAVE_PMEN_bm*/;
	TWIC.SLAVE.ADDR = 1<<1 /*| 1*/;
//
//	PORTC.DIRSET = 0xff;
//	PORTC.OUTSET = 0xff;
//	PORTC.PIN0CTRL = PORT_OPC_PULLUP_gc;
	//PORTC.PIN1CTRL = PORT_OPC_PULLUP_gc;

	while(1) {
		//if ((TWIC.SLAVE.STATUS & TWI_SLAVE_APIF_bm) ||  (TWIC.SLAVE.STATUS & TWI_SLAVE_AP_bm)) {
			//TWIC.SLAVE.CTRLB = TWI_SLAVE_CMD_RESPONSE_gc;
	//		set_lights(1);	
	//	}

//		if ( !(PORTC.IN & PIN0_bm) ) 
//			set_lights(1);
//		_delay_ms(10);
		leds_run();
	}

	return 0;
}

ISR(TWIC_TWIS_vect) {
	
    // If address match. 
	if ((TWIC.SLAVE.STATUS & TWI_SLAVE_APIF_bm) &&  (TWIC.SLAVE.STATUS & TWI_SLAVE_AP_bm)) {
		TWIC.SLAVE.CTRLB = TWI_SLAVE_CMD_RESPONSE_gc;

	}

	// If data interrupt. 
	else if (TWIC.SLAVE.STATUS & TWI_SLAVE_DIF_bm) {

		// slave write 
		if (TWIC.SLAVE.STATUS & TWI_SLAVE_DIR_bm) {

		//slave read(master write)
		} else {
			uint8_t data = TWIC.SLAVE.DATA;

			if ( data == 'A' )
				set_lights(0);
			if ( data == 'B' );
				set_lights(1);
		}
	}
	// If unexpected state. 
	else {
		//TWI_SlaveTransactionFinished(twi, TWIS_RESULT_FAIL);
	}
}


