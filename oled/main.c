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

#include "FreeRTOS.h"
#include "task.h"

static void shift_string(char *string);
static void display_scroll(char *string);

static void main_thread(void *);

int main(void) {
	sysclk_set_internal_32mhz();

	PMIC.CTRL |= PMIC_MEDLVLEN_bm | PMIC_LOLVLEN_bm | PMIC_HILVLEN_bm;

	init_timers();
	display_init();
	mastercomm_init();

	xTaskCreate(main_thread, "main", 256, NULL, tskIDLE_PRIORITY + 5, (TaskHandle_t*)NULL);
	vTaskStartScheduler();

	return 0;
}

static void main_thread(void *params) {
	wdt_enable(9);
	while (1) {
		wdt_reset();

		display_pkt *pkt = mastercomm_recv();

		if (pkt == NULL) continue;

		display_cmd(0x02);
		display_cmd(0x01);
		display_putstring((char *)pkt->data);

	}
}

static void display_scroll(char *string) {
	while (1) {
		display_cmd(0x02);
		display_tick();
//		display_clear();

		for (uint8_t i = 0; i < 16 && *(string + i) != '\0'; ++i) {
			display_write(*(string + i));
			display_tick();
		}

		shift_string(string);

		_delay_ms(125);
	}
}

static void shift_string(char *string) {
	char first = *string;

	while (1) {
		*string = *(string + 1);

		if (*string == '\0') {
			*string = first;
			break;
		}
		string++;
	}
}
