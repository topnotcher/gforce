#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <g4config.h>
#include "config.h"

#include <leds.h>
#include <colors.h>
#include <settings.h>
#include <drivers/tlc59711.h>

#include <mpc.h>


#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>

static TaskHandle_t leds_task_handle;
static TimerHandle_t led_timer;

#ifndef LED_DMA_CH
#define LED_DMA_CH -1
#endif

typedef enum {
	//lights are off
	idle,
	//idle, but requested to turn them on.
	start,

	//lights are on, and the leds are on.
	on,

	//lights are on, but this is the off period of a flash.
	off,

	//repeat wait time.
	repeat,

	stop
} led_status;

typedef struct {

	/**
	 * Pointer to led_values_t;
	 */
	const uint8_t *leds;

	union {
		led_sequence *seq;
		uint8_t *seq_data;
	} u_seq;

	uint8_t pattern;

	uint8_t flashes;

	volatile led_status status;

	uint8_t ticks;
} led_state;


//max sequence size = 58 right now @TODO.
static uint8_t led_sequence_raw[58];

static void led_set_brightness(const uint8_t);
static void led_timer_start(void);
static void led_write(void);
static void led_timer_tick(TimerHandle_t);

static void leds_run(void);
static portTASK_FUNCTION(leds_task, params);

static void set_seq_cmd(const mpc_pkt *const pkt);
static void lights_off_cmd(const mpc_pkt *const pkt);
static void set_active_color(const uint8_t color);
static void handle_setting_brightness(const void*);
static void handle_setting_active_color(const void*);
static void set_lights(uint8_t status);
static void led_set_seq(const uint8_t * const,const uint8_t);

static uint16_t colors[][3] = { COLOR_RGB_VALUES };

static tlc59711_dev *tlc;

static led_values_t ALL_OFF = LED_PATTERN(OFF, OFF, OFF, OFF, OFF, OFF, OFF, OFF);

led_state state = {

	//pointer to value to set the LEDS to.
	.leds = ALL_OFF,

	//pointer sequence being played
	.u_seq.seq = NULL,

	.status = idle,
};

static void set_lights(uint8_t status) {

	if (!status && state.ticks)
		xTimerStop(led_timer, configTICK_RATE_HZ/4);

	state.status = status ? start : stop;
	xTaskNotify(leds_task_handle, 0, eNoAction);
}

static portTASK_FUNCTION(leds_task, params) {
	led_timer = xTimerCreate("led", LED_DELAY_TIME, 0, NULL, led_timer_tick);

	set_active_color(COLOR_GREEN);

	mpc_register_cmd(MPC_CMD_LED_SET_SEQ, set_seq_cmd);
	mpc_register_cmd(MPC_CMD_LED_OFF, lights_off_cmd);

	register_setting(SETTING_BRIGHTNESS, SETTING_UINT8, handle_setting_brightness);
	register_setting(SETTING_ACTIVE_COLOR, SETTING_UINT8, handle_setting_active_color);
	set_lights(0);

	while (1) {
		if (xTaskNotifyWait(0, 0, NULL, portMAX_DELAY) == pdTRUE)
			leds_run();
	}
}

/**
 * I sincerly apologize for this mess...
 * In other languages I would have avoided the gotos
 * but I guess I don't feel like bothering in C...
 * Meh, it's not that dirty, and there's nothing wrong with using
 * gotos carefully.
 */
static void leds_run(void) {

	switch (state.status) {
	//cause we be polllin! we rollin! they hatin'!
	case idle:
		return;

	//idle->start transition
	case start:
		//@TODO remove this because it gets set elseeeewhere.
		state.pattern = 0;
		state.flashes = 0;

		/**
		 * another flash of the same pattern
		 * OR beginning a new patern
		 * OR restarting the sequence from the beginning
		 */
start:
		state.ticks = state.u_seq.seq->patterns[state.pattern].on;
		state.leds = state.u_seq.seq->patterns[state.pattern].pattern;
		state.status = on;
		led_write();
		led_timer_start();
		return;

	/* end case start */

	case on:
		//@TODO decrement this shizz asynchronously!
		if (state.ticks != 0)
			return;

		//no off time: jump to the end of the off time.
		if (state.u_seq.seq->patterns[state.pattern].off == 0) {
			goto off;
		} else {
			state.leds = ALL_OFF;
			state.status = off;
			state.ticks = state.u_seq.seq->patterns[state.pattern].off;
			led_write();
			led_timer_start();
		}

		return;
	/* end case on */

	case off:
		if (state.ticks != 0)
			return;

		//Jump from case on when off time is zero.
off:
		//done flashing.
		if (++state.flashes == state.u_seq.seq->patterns[state.pattern].flashes) {
			state.flashes = 0;
			state.pattern++;
		}

		if (state.pattern >= state.u_seq.seq->size) {
			state.pattern = 0;
			if (state.u_seq.seq->repeat_time == 0) {
				goto start;
			} else {
				state.ticks = state.u_seq.seq->repeat_time;
				state.status = repeat;
				led_timer_start();
				return;
			}
			//procede to next pattern or continue flashing the current pattern.
		} else {
			goto start;
		}

		break;

	case repeat:
		if (state.ticks != 0)
			return;

		goto start;

	case stop:
	default:
		state.status = idle;
		state.leds = ALL_OFF;
		led_write();
		break;


	}
}

void led_write(void) {
	for (uint8_t led = 0, comp = 0; led < N_LEDS; led++) {
		uint16_t const *color = colors[((led & 0x01) ? state.leds[comp++] : (state.leds[comp] >> 4)) & 0x0F];

		uint8_t idx[] = {0, 1, 2};

		#ifdef SHOULDER_V1_HW
		if (led == 2 || led == 3 || led == 6 || led == 7) {
			idx[0] = 2;
			idx[2] = 0;
		}
		#endif

		tlc59711_set_color(tlc, led, color[idx[0]], color[idx[1]], color[idx[2]]);
	}

	tlc59711_write(tlc);
}

static void set_seq_cmd(const mpc_pkt *const pkt) {
	led_set_seq(pkt->data, pkt->len);
	set_lights(1);
}

static void lights_off_cmd(const mpc_pkt *const pkt) {
	set_lights(0);
}

static void set_active_color(const uint8_t color) {
	memcpy(&colors[COLOR_ACTIVE], &colors[color], sizeof(colors[COLOR_ACTIVE]));
}

static void handle_setting_active_color(const void* setting) {
	set_active_color(*(uint8_t*)setting);
}

static void handle_setting_brightness(const void *setting) {
	led_set_brightness(*(uint8_t*)setting);
}

static void led_set_seq(const uint8_t *const data, const uint8_t len) {
	if (state.status != idle && state.status != stop)
		set_lights(0);

	// TODO: len
	memcpy((void *)led_sequence_raw, data, len);

//	if ( state.seq != NULL ) free(state.seq);

//	state.seq = seq;
}

led_spi_dev __attribute__((weak)) *led_init_driver(void) {
	return NULL;
}

void led_init(void) {
	led_spi_dev *spi;

	// why???
	state.u_seq.seq_data = led_sequence_raw;

	spi = led_init_driver();
	if (spi) {
		tlc = tlc59711_init(spi, 2);

		if (tlc) {
			xTaskCreate(leds_task, "leds", 128, NULL, tskIDLE_PRIORITY + 6, &leds_task_handle);
		}
	}
}

void led_set_brightness(const uint8_t brightness) {
	const uint8_t max = TLC59711_BRIGHTNESS_MAX;
	const uint8_t min = 0x00;

	if (brightness >= min && brightness <= max)
		tlc59711_set_brightness(tlc, TLC59711_RED | TLC59711_GREEN | TLC59711_BLUE, brightness);
}

static void led_timer_start(void) {
	xTimerChangePeriod(led_timer, LED_DELAY_TIME * state.ticks, 0);
	xTimerStart(led_timer, 0);
}

void led_timer_tick(TimerHandle_t _) {
	state.ticks = 0;
	xTaskNotify(leds_task_handle, 0, eNoAction);
}
