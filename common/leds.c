#include <util/delay.h>
#include <avr/sleep.h>
#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include <util.h>

#include <leds.h>
#include "colors.h"

#include <timer.h>
#include <mpc.h>

#include "FreeRTOS.h"
#include "task.h"

static void leds_run(void);
static portTASK_FUNCTION(leds_task, params);

static TaskHandle_t leds_task_handle;

#define _SCLK_bm G4_PIN(LED_SCLK_PIN)
#define _SOUT_bm G4_PIN(LED_SOUT_PIN)
#define _SS_bm G4_PIN(LED_SS_PIN)

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

//DERPDERP
#define LED_HEADER 0x94, 0x5F, 0xFF, 0xFF
#define LED_HEADER_BYTES 4
#define LED_TOTAL_BYTES 28

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

	/**
	 * State information for writing to the SPI
	 */
	uint8_t bytes[2][LED_TOTAL_BYTES];

	uint8_t byte;

	uint8_t controller;
} led_state;


//max sequence size = 58 right now @TODO.
static uint8_t led_sequence_raw[58];

static inline void led_timer_start(void) ATTR_ALWAYS_INLINE;
static inline void sclk_trigger(void) ATTR_ALWAYS_INLINE;
static inline void led_write(void) ATTR_ALWAYS_INLINE;
static inline void led_timer_tick(void) ATTR_ALWAYS_INLINE;
static inline void led_write_byte(void) ATTR_ALWAYS_INLINE;

static void set_seq_cmd(const mpc_pkt *const pkt);
static void lights_off_cmd(const mpc_pkt *const pkt);

const uint16_t const colors[][3] = { COLOR_RGB_VALUES };


static led_values_t ALL_OFF = LED_PATTERN(OFF, OFF, OFF, OFF, OFF, OFF, OFF, OFF);

led_state state = {

	//pointer to value to set the LEDS to.
	.leds = ALL_OFF,

	//pointer sequence being played
	.u_seq.seq = NULL,

	.status = idle,

	.bytes = {

		{LED_HEADER, 0},
		{LED_HEADER, 0}
	}
};

inline void set_lights(uint8_t status) {

	if (!status && state.ticks)
		del_timer(led_timer_tick);

	state.status = status ? start : stop;
	xTaskNotify(leds_task_handle, 0, eNoAction);
}

static portTASK_FUNCTION(leds_task, params) {
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

static inline void sclk_trigger(void) {
	LED_SPI.CTRL &= ~SPI_ENABLE_bm;
	LED_PORT.OUTSET = _SCLK_bm;
	_delay_us(1);
	LED_PORT.OUTCLR = _SCLK_bm;
	LED_SPI.CTRL |= SPI_ENABLE_bm;

}


void led_write(void) {

	uint8_t controller = 0;
	uint8_t byte = LED_HEADER_BYTES;
	//now the actual value of the LED
	for (uint8_t led = 0, comp = 0; led < N_LEDS; led++) {

		//lol hard coded :D
		if (led == 4) {
			controller = 1;
			byte = LED_HEADER_BYTES;
		}

		//this is ugly. It should be like... more lines.
		uint16_t const *color = colors[((led & 0x01) ? state.leds[comp++] : (state.leds[comp] >> 4)) & 0x0F ];

		//for each component of the color: Blue, Green, Red (MSB->LSB)
		for (int8_t c = 2; c >= 0; --c) {
			uint8_t c_real = c;

			// Software fix for backwards R/B on some LEDs
#ifdef LED_RB_SWAP
			if (led == 2 || led == 3 || led == 6 || led == 7) {
				if (c == 0) c_real = 2;
				else if (c == 2) c_real = 0;
			}
#endif
			//each component = two bytes (inorite???)
			state.bytes[controller][byte++] = (color[c_real] >> 8) & 0xFF;
			state.bytes[controller][byte++] = color[c_real] & 0xFF;
		}
	}

	state.byte = 0;
	state.controller = 0;

	led_write_byte();
}

static void set_seq_cmd(const mpc_pkt *const pkt) {
	led_set_seq(pkt->data, pkt->len);
	set_lights(1);
}

static void lights_off_cmd(const mpc_pkt *const pkt) {
	set_lights(0);
}

void led_set_seq(const uint8_t *const data, const uint8_t len) {
	if (state.status != idle && state.status != stop)
		set_lights(0);

	memcpy((void *)led_sequence_raw, data, len);

//	if ( state.seq != NULL ) free(state.seq);

//	state.seq = seq;
}

void led_init(void) {


	//SS will fuck you over hard per xmegaA, pp226.
	LED_PORT.DIRSET = _SCLK_bm | _SOUT_bm | _SS_bm;
	LED_PORT.OUTSET = _SCLK_bm | _SOUT_bm;
	LED_PORT.OUTCLR = _SS_bm;


	//32MhZ, DIV4 = 8, CLK2X => 16Mhz. = 1/16uS per bit. *8 => 1-2uS break to latch.
	LED_SPI.CTRL = SPI_ENABLE_bm | SPI_MASTER_bm | SPI_CLK2X_bm | SPI_PRESCALER_DIV4_gc;
	LED_SPI.INTCTRL = SPI_INTLVL_LO_gc;
	//state.seq = NULL/*&seq_active*/;
	state.u_seq.seq_data = led_sequence_raw;

	mpc_register_cmd('A', set_seq_cmd);
	mpc_register_cmd('B', lights_off_cmd);

	xTaskCreate(leds_task, "leds", 70, NULL, tskIDLE_PRIORITY + 1, &leds_task_handle);

	//the state is set in its own initializer way up there^
	led_write();
}

static inline void led_timer_start(void) {
	add_timer(led_timer_tick, LED_DELAY_TIME * state.ticks, 1);
}

void led_timer_tick(void) {
	state.ticks = 0;
	xTaskNotifyFromISR(leds_task_handle, 0, eNoAction, NULL);
}

static inline void led_write_byte(void) {
	if (state.byte == LED_TOTAL_BYTES) {
		if (state.controller == 1) {
			sclk_trigger(); //does this work?
			return;
		} else {
			state.controller = 1;
			state.byte = 0;
		}
	}

	LED_SPI.DATA = state.bytes[state.controller][state.byte++];
}

ISR(LED_SPI_vect) {
	led_write_byte();
}

