#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <stdlib.h>
#include <string.h>

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

#include "charger.h"

#include "FreeRTOS.h"
#include "task.h"

static portTASK_FUNCTION(main_thread, params);

int main(void) {
	sysclk_set_internal_32mhz();

	init_timers();
	mpc_init();
	buzz_init();
	tasks_init();
	task_schedule(charger_init);
	diag_init();

	PMIC.CTRL |= PMIC_LOLVLEN_bm | PMIC_MEDLVLEN_bm | /*PMIC.CTRL |*/ PMIC_HILVLEN_bm;

	xTaskCreate(main_thread, "main", 256, NULL, tskIDLE_PRIORITY + 5, (TaskHandle_t*)NULL);
	vTaskStartScheduler();

	return 0;
}

static portTASK_FUNCTION(main_thread, params) {
	led_init();
	irrx_init();

	while (1) {
		tasks_run();
	}
}
