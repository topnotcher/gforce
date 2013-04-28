#include <util/delay.h>
#include <avr/sleep.h>
#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>

#include "config.h"
#include <util.h>

#include <leds.h>
#include "colors.h"

#include "scheduler.h"

#define _SCLK_bm G4_PIN(LED_SCLK_PIN)
#define _SOUT_bm G4_PIN(LED_SOUT_PIN)



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
	const uint8_t * leds;

	led_sequence * seq;

	uint8_t pattern;

	uint8_t flashes;

	volatile led_status status;

	uint8_t ticks;
} led_state; 



static inline void led_timer_start(void);
static inline void sclk_trigger(void);
static inline void led_write(void);
static inline void led_write_header(void);
static void led_timer_tick(void);

// {command}0000{repeat}0000|{8xled}0000 0000 0000 0000 0000 0000 0000 0000 |{on time}0000 {off time}
//
const uint16_t const colors[][3] = { COLOR_RGB_VALUES };


led_values_t ALL_OFF = LED_PATTERN(OFF,OFF,OFF,OFF,OFF,OFF,OFF,OFF);

led_state state = {

	//pointer to value to set the LEDS to.
	.leds = ALL_OFF,
	
	//pointer sequence being played 
	.seq = NULL,

	.status = idle,
};

/*makes it compile as pedantic.. led_sequence seq_active = {
	.size = 8,
	.repeat_time = 0,
	.patterns = {
		{
			.pattern = LED_PATTERN(RED,OFF,RED,OFF,RED,OFF,RED,OFF),
			.flashes = 1,
			.on=6,
			.off=0,
		},

		{
			.pattern = LED_PATTERN(OFF,GREEN,OFF,GREEN,OFF,GREEN,OFF,GREEN),
			.flashes = 1,
			.on=6,
			.off=0,
		},
		{
			.pattern = LED_PATTERN(BLUE,OFF,BLUE,OFF,BLUE,OFF,BLUE,OFF),
			.flashes = 1,
			.on=6,
			.off=0,
		},
		{
			.pattern = LED_PATTERN(OFF,YELLOW,OFF,YELLOW,OFF,YELLOW,OFF,YELLOW),
			.flashes = 1,
			.on=6,
			.off=0,
		},
		{
			.pattern = LED_PATTERN(PURPLE,OFF,PURPLE,OFF,PURPLE,OFF,PURPLE,OFF),
			.flashes = 1,
			.on=6,
			.off=0,
		},
		{
			.pattern = LED_PATTERN(OFF,WHITE,OFF,WHITE,OFF,WHITE,OFF,WHITE),
			.flashes = 1,
			.on=6,
			.off=0,
		},
		{
			.pattern = LED_PATTERN(CYAN,OFF,CYAN,OFF,CYAN,OFF,CYAN,OFF),
			.flashes = 1,
			.on=6,
			.off=0,
		},
		{
			.pattern = LED_PATTERN(OFF,ORANGE,OFF,ORANGE,OFF,ORANGE,OFF,ORANGE),
			.flashes = 1,
			.on=6,
			.off=0,
		}
};*/

inline void set_lights(uint8_t status) {
	state.status = status ? start : stop;
}


/**
 * I sincerly apologize for this mess...
 * In other languages I would have avoided the gotos
 * but I guess I don't feel like bothering in C...
 * Meh, it's not that dirty, and there's nothing wrong with using
 * gotos carefully. 
 */
inline void leds_run(void) {

	switch ( state.status ) {
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
			state.ticks = state.seq->patterns[state.pattern].on;
			state.leds = state.seq->patterns[state.pattern].pattern;
			state.status = on;
			led_write();
			led_timer_start();
			return;

		/* end case start */

		case on:
			//@TODO decrement this shizz asynchronously!
			if ( state.ticks != 0 )
				return;
			
			//no off time: jump to the end of the off time.
			if ( state.seq->patterns[state.pattern].off == 0 ) {
				goto off;
			} else {
				state.leds = ALL_OFF;
				state.status = off;
				state.ticks = state.seq->patterns[state.pattern].off;
				led_write();
				led_timer_start();
			}

			return;
		/* end case on */
			
		case off:
			if ( state.ticks != 0 )
				return;

		//Jump from case on when off time is zero.
		off: 
			//done flashing.
			if ( ++state.flashes == state.seq->patterns[state.pattern].flashes ) {
				state.flashes = 0;
				state.pattern++;
			}

			if ( state.pattern >= state.seq->size ) {
				state.pattern = 0;
				if ( state.seq->repeat_time == 0 ) {
					goto start;
				} else {
					state.ticks = state.seq->repeat_time;
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
			if ( state.ticks != 0 )
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
	LED_PORT.OUTSET = _SCLK_bm;
	LED_PORT.OUTCLR = _SCLK_bm;
}

static inline void led_write_header(void) {
	uint8_t byte = 0x25;
	
	for ( uint8_t i = 0; i < 6; ++i ) {	
		//MSB->LSB: 5,4,3,2,1,0
		if ( (byte>>(5-i))&0x01 ) 
			LED_PORT.OUTSET = _SOUT_bm;
		else
			LED_PORT.OUTCLR = _SOUT_bm;

		sclk_trigger();
	}


	//next five bits = function control
	byte = 0x02;//0b00000010;

	for ( uint8_t i = 0; i < 5; ++i )  {
		if ( (byte>>(4-i))&0x01 )
			LED_PORT.OUTSET = _SOUT_bm;
		else
			LED_PORT.OUTCLR = _SOUT_bm;

		sclk_trigger();
	}
	

	//brightness control bits - 7 per color.
	//@TODO why does sclk_trigger set the pin low???
	LED_PORT.OUTSET = _SOUT_bm;
	for ( uint8_t i = 0; i < 21; ++i ){
		sclk_trigger();
	}
}

void led_write(void) {	
	//since we're bit-banging...
	cli();

	//now the actual value of the LED
	for ( uint8_t led = 0,comp = 0; led < N_LEDS; led++ ) {
		
		if ( led == 0 || led == 4 )
			led_write_header();

		//this is ugly. It should be like... more lines.
		uint16_t const * color = colors[( (led&0x01 ) ? state.leds[comp++] : (state.leds[comp]>>4) ) & 0x0F ];
		
		//for each component of the color: Blue, Green, Red (MSB->LSB)
		for ( int8_t c = 2; c>=0; --c ) {
			uint8_t c_real = c;

			
			/** Software fix for backwards R/B on some LEDs **/
#ifdef LED_RB_SWAP
			if ( led == 2 || led == 3 || led == 6 || led == 7 ) {
				if ( c == 0 ) c_real = 2;
				else if ( c == 2) c_real = 0;
			}
#endif

			//each bit in color
			//NOTE: this relies on the AVR endianness!!!
			for ( int8_t bit = 15; bit >= 0; --bit ) {
				if ( (color[c_real]>>bit)&0x01 )
					LED_PORT.OUTSET = _SOUT_bm;
				else
					LED_PORT.OUTCLR = _SOUT_bm;

				sclk_trigger();
			}
		}
	}

	/**
	 * in THEORY, keeping clk low for this amount of time
	 * SHOULD make the data latch...
	 * The requirement is something like 8 times the sclk_trigger() time.
	 */
	_delay_us(50);

	sclk_trigger();

	sei();
}

void led_set_seq(led_sequence * seq) {
	//NOTE: BAD shit could happen here if lights are enabled when we do this.
	if ( state.seq != NULL /*&& state.seq != &seq_active*/ ) free(state.seq);

	//fix setting seq while it's already on.
	if ( state.status != idle && state.status != stop ) 
		state.status = start;

	state.seq = seq;
}

void led_init(void) {


	LED_PORT.DIRSET = _SCLK_bm | _SOUT_bm;
	

	LED_PORT.OUTCLR = _SCLK_bm | _SOUT_bm;

	state.seq = NULL/*&seq_active*/;

	//the state is set in its own initializer way up there^
	led_write();

 	TCC0.CTRLA |= TC_CLKSEL_DIV8_gc; 

	//enable compare
	TCC0.CTRLB |= TC0_CCAEN_bm;

}

static inline void led_timer_start(void) {
	TCC0.CNT = 0;	
	TCC0.CCA = 55000;
	TCC0.INTCTRLB |= TC_CCAINTLVL_MED_gc;
}

static inline void led_timer_tick(void) {
	if ( --state.ticks == 0 )
		TCC0.INTCTRLB &= ~TC_CCAINTLVL_MED_gc;

}

ISR(TCC0_CCA_vect) {
	led_timer_tick();
}
