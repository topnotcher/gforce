#include <stdint.h>

#include "colors.h"

#ifndef LED_PATTERNS_H
#define LED_PATTERNS_H

#define N_LEDS 8 

#define LED_PATTERN(A,B,C,D,E,F,G,H) { (COLOR(A)<<4) | COLOR(B), (COLOR(C)<<4) | COLOR(D), (COLOR(E)<<4) | COLOR(F), (COLOR(G)<<4) | COLOR(H) }
#define SOLID_PATTERN(C) {(COLOR(C)<<4 | COLOR(C)) , (COLOR(C)<<4 | COLOR(C)) , (COLOR(C)<<4 | COLOR(C)) , (COLOR(C)<<4 | COLOR(C))}

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

#endif
