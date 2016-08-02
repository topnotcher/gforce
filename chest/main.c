#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <stdlib.h>
#include <string.h>

#include <g4config.h>
#include "config.h"


/**
 * G4 common includes.
 */
#include <mpc.h>
#include <leds.h>
#include <buzz.h>
#include <irrx.h>
#include <tasks.h>
#include <util.h>
#include <timer.h>
#include <diag.h>

int main(void) {
	sysclk_set_internal_32mhz();

	init_timers();
	mpc_init();
	led_init();
	buzz_init();
	irrx_init();
	tasks_init();
	diag_init();

	PMIC.CTRL |= PMIC_LOLVLEN_bm | PMIC_MEDLVLEN_bm | /*PMIC.CTRL |*/ PMIC_HILVLEN_bm;
	sei();

	while (1) {
		tasks_run();
	}


	return 0;
}
