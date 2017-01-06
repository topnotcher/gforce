#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <stdlib.h>
#include <string.h>

/**
 * G4 common includes.
 */
#include <mpc.h>
#include "mpctwi.h"
#include <leds.h>
#include <buzz.h>
#include <irrx.h>
#include <diag.h>

#include "FreeRTOS.h"
#include "task.h"

#include "main.h"

int main(void) {
	system_board_init();
	system_configure_os_ticks();
	system_configure_interrupts();

	mpc_init();
	mpc_register_driver(mpctwi_init());
	buzz_init();
	diag_init();

	led_init();
	irrx_init();

	vTaskStartScheduler();

	return 0;
}
