#include <stdbool.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#include "config.h"
#include "trigger.h"
#include <util.h>

#include <g4config.h>
#include <mpc.h>
#include "tasks.h"

#include "FreeRTOS.h"
#include "timers.h"

#define _TRIGPINCTRL G4_PINCTRL(TRIGGER_PIN)
#define _TRIGPIN_bm G4_PIN(TRIGGER_PIN)

static void trigger_disable(void);
static void trigger_enable(void);
static void trigger_pressed(void);
static void trigger_tick(TimerHandle_t);

static volatile bool _trigger_pressed;
static volatile bool _trigger_enabled;
static TimerHandle_t trigger_timer;

//static volatile uint8_t ticks = 0;

void trigger_init(void) {
	TRIGGER_PORT.DIRCLR = _TRIGPIN_bm;
	TRIGGER_PORT._TRIGPINCTRL = PORT_OPC_PULLUP_gc | PORT_ISC_LEVEL_gc;

	_trigger_pressed = false;
	_trigger_enabled = true;

	// TODO period.
	trigger_timer = xTimerCreate("trigger", configTICK_RATE_HZ/2, 0, NULL, trigger_tick);

	//configure interrupts
	TRIGGER_PORT.INT0MASK = _TRIGPIN_bm;
	trigger_enable();
}

static void trigger_pressed(void) {
	if (_trigger_pressed) {
		_trigger_pressed = false;
		xTimerStart(trigger_timer, 0);
		mpc_send_cmd(MPC_ADDR_MASTER, MPC_CMD_IR_TX);
	}
}

static void trigger_enable(void) {
	_trigger_enabled = true;
	TRIGGER_PORT.INTCTRL |= PORT_INT0LVL_MED_gc;
}
static void trigger_disable(void) {
	_trigger_enabled = false;
	TRIGGER_PORT.INTCTRL &= ~PORT_INT0LVL_MED_gc;
}

ISR(PORTA_INT0_vect) {
	if (!(TRIGGER_PORT.IN & _TRIGPIN_bm) /*&&  trigger_enabled()*/) {
		_trigger_pressed = true;
		task_schedule_isr(trigger_pressed);
		trigger_disable();
	}
}

void trigger_tick(TimerHandle_t _) {
	trigger_enable();
}
