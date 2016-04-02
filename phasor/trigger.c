#include <stdbool.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#include "config.h"
#include "trigger.h"
#include <util.h>
#include <timer.h>

#define _TRIGPINCTRL G4_PINCTRL(TRIGGER_PIN)
#define _TRIGPIN_bm G4_PIN(TRIGGER_PIN)

static inline bool trigger_enabled(void);
static inline void trigger_disable(void);
static inline void trigger_enable(void);
static void trigger_tick(void);

static volatile bool _trigger_pressed;
static volatile bool _trigger_enabled;

//static volatile uint8_t ticks = 0;

inline void trigger_init(void) {
	TRIGGER_PORT.DIRCLR = _TRIGPIN_bm;
	TRIGGER_PORT._TRIGPINCTRL = PORT_OPC_PULLUP_gc | PORT_ISC_LEVEL_gc;

	_trigger_pressed = false;
	_trigger_enabled = true;

	//configure interrupts
	TRIGGER_PORT.INT0MASK = _TRIGPIN_bm;
	trigger_enable();
}

inline bool trigger_pressed(void) {
	if (_trigger_pressed) {
		_trigger_pressed = false;
		add_timer(trigger_tick, 500, 1);
		return true;
	}
	return false;
}
static inline void trigger_enable(void) {
	_trigger_enabled = true;
	TRIGGER_PORT.INTCTRL |= PORT_INT0LVL_MED_gc;
}
static inline void trigger_disable(void) {
	_trigger_enabled = false;
	TRIGGER_PORT.INTCTRL &= ~PORT_INT0LVL_MED_gc;
}
static inline bool trigger_enabled(void) {
	return _trigger_enabled;
}

ISR(PORTA_INT0_vect) {
	if (!(TRIGGER_PORT.IN & _TRIGPIN_bm) /*&&  trigger_enabled()*/) {
		_trigger_pressed = true;
		trigger_disable();
	}
}

void trigger_tick(void) {
	trigger_enable();
}
