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

	#if DEBUG == 0
	/** 
	 * Setting all three bits is extended standby.
	 * Setting no bits is idle. Dicks get sucked if you try to
	 * use USART in extended standby. (no wakey wakey!!)
	 */
//	SMCR = /*_BV(SM1) |  _BV(SM2) | _BV(SM0) |*/ _BV(SE);
	#endif

/*CTRLA Interrupt enable bits:	TWI_MASTER_WIEN_bm*/

TWIC.MASTER.CTRLA |= TWI_MASTER_INTLVL_MED_gc | TWI_MASTER_ENABLE_bm | TWI_MASTER_WIEN_bm;
/*CTRLB SMEN: look into, but only in master read*/
/*CTRLC ACKACT ... master read*/
/*CTRLC: CMD bits are here. HRRERM*/
/* BAUD: 32000000/(2*100000) - 5 = 155*/
	TWIC.MASTER.BAUD = 155/*F_CPU/(2*MPU_TWI_BAUD) - 5*/;

	//per AVR1308
	TWIC.MASTER.STATUS = TWI_MASTER_BUSSTATE_IDLE_gc;

	//set slave address = 1 and write, W = 0 (R=1)
	TWIC.MASTER.ADDR = 1<<1 | 0;

//	TWIC.MASTER.DATA= 'A';
//	PORTC.DIRSET |= 0xff;
	while (1) {

//		TWIC.MASTER.DATA= 'A';
//		PORTC.OUTSET = 0xff;

		
		//avoid sleeping for 5 seconds because
		//it's likely that shit will happen
		//this doesn't matter...
		_delay_ms(500);

//		PORTC.OUTCLR = 0xff;
		#if DEBUG == 0
//		sleep_cpu();
		#endif
	}

	return 0;
}

ISR(TWIC_TWIM_vect) {
	if ( TWIC.MASTER.STATUS & TWI_MASTER_WIF_bm ) {
		//when it is read as 0, most recent ack bit was NAK. 
		if (TWIC.MASTER.STATUS & TWI_MASTER_RXACK_bm) 
			TWIC.MASTER.CTRLC = TWI_MASTER_CMD_STOP_gc;

		else {
			//if bytes to send: send bytes
			//otherweise : twi->interface->MASTER.CTRLC = TWI_MASTER_CMD_STOP_gc
			TWIC.MASTER.DATA = 'A';

			TWIC.MASTER.CTRLC = TWI_MASTER_CMD_STOP_gc;
		}
		
		while ( (TWIC.MASTER.STATUS & TWI_MASTER_BUSSTATE_gm) != TWI_MASTER_BUSSTATE_IDLE_gc );

		TWIC.MASTER.ADDR = 1<<1 | 0;

	//IDFK??
	} else {
		TWIC.MASTER.CTRLC = TWI_MASTER_CMD_STOP_gc;

		//fail
		return;
	}
}
