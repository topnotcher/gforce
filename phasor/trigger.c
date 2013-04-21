#include <stdbool.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#include "config.h"
#include "trigger.h"
#include <util.h>

#define _TRIGPINCTRL G4_PINCTRL(TRIGGER_PIN)
#define _TRIGPIN_bm G4_PIN(TRIGGER_PIN)

static bool trigger_enabled(void);
static void trigger_disable(void);
static void trigger_enable(void);


static volatile bool _trigger_pressed;
static volatile bool _trigger_enabled;

inline void trigger_init() {
	TRIGGER_PORT.DIRCLR = _TRIGPIN_bm;
	TRIGGER_PORT._TRIGPINCTRL = PORT_OPC_PULLUP_gc;

	_trigger_pressed = false;
	_trigger_enabled = true;

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
static inline void trigger_enable() {
	_trigger_enabled = true;
}
static inline void trigger_disable() {
	_trigger_enabled = false;
}
static inline bool trigger_enabled() {
	return _trigger_enabled;
}

ISR(PORTA_INT0_vect) {
	if ( !(TRIGGER_PORT.IN & _TRIGPIN_bm) &&  trigger_enabled() ) 
		_trigger_pressed = true;
}
