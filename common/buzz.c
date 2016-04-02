#include <avr/io.h>

#include <buzz.h>

inline void buzz_init(void) {
	BUZZ_PORT.DIRSET = BUZZ_PIN;
}

inline void buzz_on(void) {
	BUZZ_PORT.OUTSET = BUZZ_PIN;
}

inline void buzz_off(void) {
	BUZZ_PORT.OUTCLR = BUZZ_PIN;
}
