#include <avr/io.h>
#include <avr/interrupt.h>

#include "config.h"
#include "trigger.h"

static bool _trigger_pressed;

inline void trigger_init() {
	TRIGGER_PORT.DIRCLR = TRIGGER_PIN;
	TRIGGER_PORT.PIN2CTRL = PORT_OPC_PULLUP_gc | PORT_ISC_FALLING_gc;

	_trigger_pressed = false;

	//configure interrupts
	TRIGGER_PORT.INT0MASK = TRIGGER_PIN;
	TRIGGER_PORT.INTCTRL = PORT_INT0LVL_MED_gc;
}

inline bool trigger_pressed() {
	if ( _trigger_pressed ) {
		_trigger_pressed = false;
		return true;
	}
	return false;
}

ISR(PORTA_INT0_vect) {
	if ( ! (TRIGGER_PORT.IN & TRIGGER_PIN) )
		_trigger_pressed = true;
}
