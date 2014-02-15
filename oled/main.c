#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <stdlib.h>
#include <string.h>

#include <displaycomm.h>

#include "display.h"
#include "mastercomm.h"

#define CLKSYS_Enable( _oscSel ) ( OSC.CTRL |= (_oscSel) )

#define CLKSYS_IsReady( _oscSel ) ( OSC.STATUS & (_oscSel) )

static inline void shift_string(char * string);
static inline void display_scroll(char * string);


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

	PMIC.CTRL |= PMIC_MEDLVLEN_bm | PMIC_LOLVLEN_bm | PMIC_HILVLEN_bm;

	
	display_init();
	mastercomm_init();

	wdt_enable(9);

	sei();

	while(1) {
		wdt_reset();

		display_pkt * pkt = mastercomm_recv();
//		display_tick();

		if (pkt == NULL) continue;

		display_cmd(0x02); display_tick();
		display_cmd(0x01); 
		for (uint8_t i = 0; i < 15; ++i) {
			display_tick();
			_delay_ms(1);
		}
//		display_clear();
//		display_tick();display_tick();
		//_delay_ms(1);
		display_putstring((char *)pkt->data);
		
		for (uint8_t i = 0; i < pkt->size; ++i)
			display_tick();

	}
//	display_putstring("      eep!");
//	for ( uint8_t i = 0; i<25; ++i ) display_tick();
//	_delay_ms(1000);
//	char string[] = "      eep!      ";
//	display_scroll(string);

	return 0;
}



static inline void display_scroll(char * string) {
	while (1) {
		display_cmd(0x02);
		display_tick();
//		display_clear();
			
		for ( uint8_t i = 0; i < 16 && *(string+i) != '\0'; ++i ) {
			display_write(*(string+i));
			display_tick();
		}

		shift_string(string);
		
		_delay_ms(125);
	}
}

static inline void shift_string(char * string) {
	char first = *string;

	while ( 1 ) {
		*string = *(string + 1);

		if (*string == '\0') {
			*string = first;
			break;
		}
		string++;
	}
}
