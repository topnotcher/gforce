#include <util/delay.h>
#include <avr/sleep.h>
#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>

#include "leds.h"
#include "colors.h"

#include "config.h"

// {command}0000{repeat}0000|{8xled}0000 0000 0000 0000 0000 0000 0000 0000 |{on time}0000 {off time}
//
const uint16_t const colors[][3] = { COLOR_RGB_VALUES };


led_values_t ALL_OFF = {COLOR_OFF<<4|COLOR_OFF, COLOR_OFF<<4|COLOR_OFF, COLOR_OFF<<4|COLOR_OFF, COLOR_OFF<<4|COLOR_OFF };

led_state state = {

	//pointer to value to set the LEDS to.
	.leds = ALL_OFF,
	
	//pointer sequence being played 
	.seq = NULL,

	.status = idle,
};

/*
const led_sequence seq_active = {
	.size = 2,
	.repeat_time = 0,
	.patterns = {
		{
			.pattern = { (COLOR_BLUE<<4 | COLOR_BLUE) , (COLOR_BLUE<<4 | COLOR_OFF) , (COLOR_BLUE<<4 | COLOR_OFF) , (COLOR_BLUE<<4 | COLOR_OFF) },
			.flashes = 50,
			.on = 25,
			.off = 15,
		},
		{
			.pattern = { (COLOR_CYAN<<4 | COLOR_BLUE) , (COLOR_BLUE<<4 | COLOR_BLUE) , (COLOR_BLUE<<4 | COLOR_BLUE) , (COLOR_BLUE<<4 | COLOR_BLUE) },
			.flashes = 50,
			.on = 25,
			.off = 15
		},

	}
};
*/

#define SOLID_PATTERN(C) {(COLOR(C)<<4 | COLOR(C)) , (COLOR(C)<<4 | COLOR(C)) , (COLOR(C)<<4 | COLOR(C)) , (COLOR(C)<<4 | COLOR(C))}

const led_sequence seq_active = {
	.size = 7,
	.repeat_time = 0,
	.patterns = {
		{
			.pattern = { COLOR_RED << 4 | COLOR_OFF, COLOR_RED << 4 | COLOR_OFF, COLOR_RED << 4 | COLOR_OFF, COLOR_RED << 4 | COLOR_OFF },
			.flashes = 1,
			.on = 2,
			.off = 1,
		},

		{
			.pattern = { COLOR_OFF << 4 | COLOR_GREEN, COLOR_OFF << 4 | COLOR_GREEN, COLOR_OFF << 4 | COLOR_GREEN, COLOR_OFF << 4 | COLOR_GREEN },
			.flashes = 1,
			.on = 2,
			.off = 1,
		},
		{
			.pattern = { COLOR_BLUE << 4 | COLOR_OFF, COLOR_BLUE << 4 | COLOR_OFF, COLOR_BLUE << 4 | COLOR_OFF, COLOR_BLUE << 4 | COLOR_OFF },
			.flashes = 1,
			.on = 2,
			.off = 1,
		},
		{
			.pattern = { COLOR_OFF << 4 | COLOR_YELLOW, COLOR_OFF << 4 | COLOR_YELLOW, COLOR_OFF << 4 | COLOR_YELLOW, COLOR_OFF << 4 | COLOR_YELLOW},
			.flashes = 1,
			.on = 2,
			.off = 1,
		},
		{
			.pattern = { COLOR_PURPLE << 4 | COLOR_OFF, COLOR_PURPLE << 4 | COLOR_OFF, COLOR_PURPLE << 4 | COLOR_OFF, COLOR_PURPLE << 4 | COLOR_OFF },
			.flashes = 1,
			.on = 2,
			.off = 1,
		},
		{
			.pattern = { COLOR_OFF << 4 | COLOR_WHITE, COLOR_OFF << 4 | COLOR_WHITE, COLOR_OFF << 4 | COLOR_WHITE, COLOR_OFF << 4 | COLOR_WHITE },
			.flashes = 1,
			.on = 2,
			.off = 1,
		},
		{
			.pattern = { COLOR_ORANGE << 4 | COLOR_OFF, COLOR_ORANGE << 4 | COLOR_OFF, COLOR_ORANGE << 4 | COLOR_OFF, COLOR_ORANGE << 4 | COLOR_OFF },
			.flashes = 1,
			.on = 2,
			.off = 1,
		}/*,
		{
			.pattern = 
			.flashes = 1,
			.on = 25,
			.off = 15,
		},*/

	}
};

/*
const led_sequence seq_active = {
	.size = 8,
	.repeat_time = 0,
	.patterns = {
		{
			.pattern = SOLID_PATTERN(RED),
			.flashes = 5,
			.on = 15,
			.off = 0,
		},

		{
			.pattern = SOLID_PATTERN(BLUE),
			.flashes = 5,
			.on = 15,
			.off = 0,
		},
		{
			.pattern = SOLID_PATTERN(GREEN),
			.flashes = 5,
			.on = 15,
			.off = 0,
		},
		{
			.pattern = SOLID_PATTERN(YELLOW),
			.flashes = 5,
			.on = 15,
			.off = 0,
		},
		{
			.pattern = SOLID_PATTERN(PURPLE),
			.flashes = 5,
			.on = 15,
			.off = 0,
		},
		{
			.pattern = SOLID_PATTERN(CYAN),
			.flashes = 5,
			.on = 15,
			.off = 0,
		},
		{
			.pattern = SOLID_PATTERN(PINK),
			.flashes = 5,
			.on = 15,
			.off = 0,
		},
		{
			.pattern = SOLID_PATTERN(ORANGE),
			.flashes = 5,
			.on = 15,
			.off = 0,
		},

	}
};
*/

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
			//state.seq = &seq_active;
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
			state.status = idle;
			state.leds = ALL_OFF;
			led_write();
			break;
	}
}

inline void sclk_trigger() {
	LED_PORT.OUTSET = LED_SCLK;
	LED_PORT.OUTCLR = LED_SCLK;
}

inline void led_write_header(void) {
	uint8_t byte = 0x25;
	
	for ( uint8_t i = 0; i < 6; ++i ) {	
		//MSB->LSB: 5,4,3,2,1,0
		if ( (byte>>(5-i))&0x01 ) 
			LED_PORT.OUTSET = LED_SOUT;
		else
			LED_PORT.OUTCLR = LED_SOUT;

		sclk_trigger();
	}


	//next five bits = function control
	byte = 0b00000010;

	for ( uint8_t i = 0; i < 5; ++i )  {
		if ( (byte>>(4-i))&0x01 )
			LED_PORT.OUTSET = LED_SOUT;
		else
			LED_PORT.OUTCLR = LED_SOUT;

		sclk_trigger();
	}
	

	//brightness control bits - 7 per color.
	//@TODO why does sclk_trigger set the pin low???
	LED_PORT.OUTSET = LED_SOUT;
	for ( uint8_t i = 0; i < 21; ++i ){
		sclk_trigger();
	}
}

void led_write() {	
	//since we're bit-banging...
	cli();

	//now the actual value of the LED
	for ( uint8_t led = 0,c = 0; led < N_LEDS; led++ ) {
		
		if ( led == 0 || led == 4 )
			led_write_header();

		//this is ugly. It should be like... more lines.
		uint16_t const * color = colors[( (led&0x01 ) ? state.leds[c++] : (state.leds[c]>>4) ) & 0x0F ];
		
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
					LED_PORT.OUTSET = LED_SOUT;
				else
					LED_PORT.OUTCLR = LED_SOUT;

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
	if ( state.seq != NULL ) free(state.seq);

	//fix setting seq while it's already on.
	if ( state.status != idle && state.status != stop ) 
		state.status = start;

	state.seq = seq;
}

void led_init() {


	LED_PORT.DIRSET = LED_SCLK | LED_SOUT;
	

	LED_PORT.OUTCLR = LED_SCLK | LED_SOUT;

	state.seq = /*NULL;*/ &seq_active;

	//the state is set in its own initializer way up there^
	led_write();

	//don't prescale
 	TCC0.CTRLA |= TC_CLKSEL_DIV8_gc; 

	//enable compare
	TCC0.CTRLB |= TC0_CCAEN_bm;

}

inline void led_timer_start() {
	TCC0.CNT = 0;	
	TCC0.CCA = 40000;
	TCC0.INTCTRLB |= TC_CCAINTLVL_MED_gc;
}

ISR(TCC0_CCA_vect) {
	if ( --state.ticks == 0 )
		TCC0.INTCTRLB &= ~TC_CCAINTLVL_MED_gc;

}
