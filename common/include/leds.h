#include <stdint.h>

#ifndef LEDS_H
#define LEDS_H

#define N_LEDS 8 

#define LED(k) _BV(k)

#define LED_DELAY_TIME 13

#include <colors.h>
#define LED_PATTERN(A,B,C,D,E,F,G,H) { (COLOR(A)<<4) | COLOR(B), (COLOR(C)<<4) | COLOR(D), (COLOR(E)<<4) | COLOR(F), (COLOR(G)<<4) | COLOR(H) }
#define SOLID_PATTERN(C) {(COLOR(C)<<4 | COLOR(C)) , (COLOR(C)<<4 | COLOR(C)) , (COLOR(C)<<4 | COLOR(C)) , (COLOR(C)<<4 | COLOR(C))}


/*typedef struct { 
	PORT_t * port;
	uint8_t sout;
	uint8_t sclk;
} led_config_t;*/


typedef uint8_t led_values_t[N_LEDS/2];

typedef struct {
	led_values_t pattern;
	uint8_t flashes;
	uint8_t on;
	uint8_t off;
} led_pattern;

typedef struct {
	uint8_t size;
	uint8_t repeat_time;
	led_pattern patterns[];
} led_sequence;





//void led_on(uint8_t n);
//void led_off(uint8_t n);
void led_init(void);
//void set_state(void);
void set_lights(uint8_t);
void led_set_seq(const uint8_t * const,const uint8_t);

#endif
