#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <stdlib.h>
#include <string.h>

#include "lcd.h"

#define CLKSYS_Enable( _oscSel ) ( OSC.CTRL |= (_oscSel) )

#define CLKSYS_IsReady( _oscSel ) ( OSC.STATUS & (_oscSel) )

static inline void shift_string(char * string);
static inline void lcd_scroll(char * string);


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

	
	lcd_init();
	sei();
//	lcd_putstring("      eep!");
//	for ( uint8_t i = 0; i<25; ++i ) lcd_tick();
//	_delay_ms(1000);
	char string[] = "      eep!      ";
	lcd_scroll(string);

	return 0;
}

static inline void lcd_scroll(char * string) {
	while (1) {
		lcd_cmd(0x02);
		lcd_tick();
//		lcd_clear();
			
		for ( uint8_t i = 0; i < 16 && *(string+i) != '\0'; ++i ) {
			lcd_write(*(string+i));
			lcd_tick();
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
