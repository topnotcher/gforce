#include <stdint.h>
#include <saml21/io.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "main.h"

static void led_task(void *);

system_init_func(system_software_init) {
	// LED0 on xplained board.
	PORT[0].Group[1].DIRSET.reg = 1 << 10;
	PORT[0].Group[1].OUTCLR.reg = 1 << 10;

	xTaskCreate(led_task, "led task", 256, NULL, tskIDLE_PRIORITY + 1, NULL);
}

static void led_task(void *params) {
	while (1) {
		PORT[0].Group[1].OUTTGL.reg = 1 << 10;
		vTaskDelay(configTICK_RATE_HZ / 2);
	}
}
