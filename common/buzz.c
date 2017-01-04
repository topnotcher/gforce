#include <avr/io.h>

#include <buzz.h>

void buzz_init(void) {
	BUZZ_PORT.DIRSET = BUZZ_PIN;
}

void buzz_on(void) {
	BUZZ_PORT.OUTSET = BUZZ_PIN;
}

void buzz_off(void) {
	BUZZ_PORT.OUTCLR = BUZZ_PIN;
}
