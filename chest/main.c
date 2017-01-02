#include <avr/io.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <g4config.h>
#include "config.h"


/**
 * G4 common includes.
 */
#include <mpc.h>
#include "mpctwi.h"
#include <leds.h>
#include <buzz.h>
#include <irrx.h>
#include <util.h>
#include <timer.h>
#include <diag.h>


#include "FreeRTOS.h"
#include "task.h"

int main(void) {
	sysclk_set_internal_32mhz();

	init_timers();
	mpc_init();
	mpc_register_driver(mpctwi_init());
	buzz_init();
	diag_init();

	PMIC.CTRL |= PMIC_LOLVLEN_bm | PMIC_MEDLVLEN_bm | /*PMIC.CTRL |*/ PMIC_HILVLEN_bm;

	led_init();
	irrx_init();

	vTaskStartScheduler();

	return 0;
}
