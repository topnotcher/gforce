#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <stdlib.h>
#include <string.h>
#include <util.h>
#include <displaycomm.h>
#include <timer.h>

#include "display.h"
#include "mastercomm.h"

#define CLKSYS_Enable( _oscSel ) ( OSC.CTRL |= (_oscSel) )

#define CLKSYS_IsReady( _oscSel ) ( OSC.STATUS & (_oscSel) )

static inline void shift_string(char * string);
static inline void display_scroll(char * string);


int main(void) {
	sysclk_set_internal_32mhz();

	PMIC.CTRL |= PMIC_MEDLVLEN_bm | PMIC_LOLVLEN_bm | PMIC_HILVLEN_bm;

	timer_init();
	display_init();
	mastercomm_init();

	wdt_enable(9);

	sei();
	//display_putstring("derp");
	while(1) {
		wdt_reset();

		display_pkt * pkt = mastercomm_recv();

		if (pkt == NULL) continue;

		display_cmd(0x02); 
		display_cmd(0x01); 
		display_putstring((char *)pkt->data);

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
