#include <avr/io.h>


#ifndef LEDS_H
#define LEDS_H


/**
 * Port/pin definitions
 */
//#define LED_PORT PORTD

//LE
//#define SCLK PIN7_bm
//#define SOUT PIN5_bm
#define N_LEDS 8 

#define LED_PIN_HIGH(pin) LED_PORT.OUTSET |= pin
#define LED_PIN_LOW(pin) LED_PORT.OUTCLR |= pin

#define LED(k) _BV(k)

#define DELAY_TIME 11

#include <colors.h>
#define LED_PATTERN(A,B,C,D,E,F,G,H) { (COLOR(A)<<4) | COLOR(B), (COLOR(C)<<4) | COLOR(D), (COLOR(E)<<4) | COLOR(F), (COLOR(G)<<4) | COLOR(H) }
#define SOLID_PATTERN(C) {(COLOR(C)<<4 | COLOR(C)) , (COLOR(C)<<4 | COLOR(C)) , (COLOR(C)<<4 | COLOR(C)) , (COLOR(C)<<4 | COLOR(C))}


/*typedef struct { 
	PORT_t * port;
	uint8_t sout;
	uint8_t sclk;
} led_config_t;*/


typedef uint8_t led_values_t[N_LEDS/2];

typedef const struct {
	led_values_t pattern;
	uint8_t flashes;
	uint8_t on;
	uint8_t off;
} led_pattern;

typedef const struct {
	uint8_t size;
	uint8_t repeat_time;
	led_pattern patterns[];
} led_sequence;

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

	const led_sequence * volatile seq;

	uint8_t pattern;

	uint8_t flashes;

	volatile led_status status;

	uint8_t ticks;
} led_state; 



//void led_on(uint8_t n);
//void led_off(uint8_t n);
void xlat_trigger(void);
void sclk_trigger(void);
void led_write(void);
void led_init(void);
//void set_state(void);
void set_lights(uint8_t);
void leds_run(void);
void led_set_seq(led_sequence *);
void led_timer_start(void);
#endif
