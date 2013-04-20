#include <avr/io.h>

#include <buzz.h>

inline void buzz_init() {
	BUZZ_PORT.DIRSET = BUZZ_PIN;
}

inline void buzz_on() {
	BUZZ_PORT.OUTSET = BUZZ_PIN;
}

inline void buzz_off() {
	BUZZ_PORT.OUTCLR = BUZZ_PIN;
}
