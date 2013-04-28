#include <stdbool.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#include "config.h"
#include "trigger.h"
#include <util.h>

#define _TRIGPINCTRL G4_PINCTRL(TRIGGER_PIN)
#define _TRIGPIN_bm G4_PIN(TRIGGER_PIN)

static inline bool trigger_enabled(void);
static inline void trigger_disable(void);
static inline void trigger_enable(void);


static volatile bool _trigger_pressed;
static volatile bool _trigger_enabled;

//static volatile uint8_t ticks = 0;

inline void trigger_init(void) {
	TRIGGER_PORT.DIRCLR = _TRIGPIN_bm;
	TRIGGER_PORT._TRIGPINCTRL = PORT_OPC_PULLUP_gc;

	_trigger_pressed = false;
	_trigger_enabled = true;

	RTC.CTRL = RTC_PRESCALER_DIV1_gc;
	CLK.RTCCTRL = CLK_RTCSRC_ULP_gc | CLK_RTCEN_bm;

	//configure interrupts
	TRIGGER_PORT.INT0MASK = _TRIGPIN_bm;
	trigger_enable();
}

inline bool trigger_pressed(void) {
	if ( _trigger_pressed ) {
		_trigger_pressed = false;
		return true;
	}
	return false;
}
static inline void trigger_enable(void) {
	_trigger_enabled = true;
	TRIGGER_PORT.INTCTRL = PORT_INT0LVL_MED_gc;
}
static inline void trigger_disable(void) {
	_trigger_enabled = false;
	TRIGGER_PORT.INTCTRL &= ~PORT_INT0LVL_MED_gc;
}
static inline bool trigger_enabled(void) {
	return _trigger_enabled;
}

ISR(PORTA_INT0_vect) {
	if ( !(TRIGGER_PORT.IN & _TRIGPIN_bm) &&  trigger_enabled() ) {

		RTC.COMP = 500;
		RTC.CNT = 0;
		RTC.INTCTRL = RTC_COMPINTLVL_MED_gc;
	
		trigger_disable();

		_trigger_pressed = true;

	}
}

ISR(RTC_COMP_vect) {
//	RTC.CNT = 0;
//	if ( ++ticks == 500 ) 
	trigger_enable();
	RTC.INTCTRL &= ~RTC_COMPINTLVL_MED_gc;
}
