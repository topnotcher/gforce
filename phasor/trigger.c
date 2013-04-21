#include <avr/io.h>
#include <avr/interrupt.h>

#include "config.h"
#include "trigger.h"
#include <util.h>

#define _TRIGPINCTRL G4_PINCTRL(TRIGGER_PIN)
#define _TRIGPIN_bm G4_PIN(TRIGGER_PIN)

static bool _trigger_pressed;

inline void trigger_init() {
	TRIGGER_PORT.DIRCLR = _TRIGPIN_bm;
	TRIGGER_PORT._TRIGPINCTRL = PORT_OPC_PULLUP_gc | PORT_ISC_FALLING_gc;

	_trigger_pressed = false;

	//configure interrupts
	TRIGGER_PORT.INT0MASK = _TRIGPIN_bm;
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
	if ( ! (TRIGGER_PORT.IN & _TRIGPIN_bm) )
		_trigger_pressed = true;
}
