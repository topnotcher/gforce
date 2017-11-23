#include <stdbool.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#include "config.h"
#include "trigger.h"
#include <drivers/xmega/util.h>

#include <g4config.h>
#include <mpc.h>
#include "tasks.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>

#define _TRIGPINCTRL G4_PINCTRL(TRIGGER_PIN)
#define _TRIGPIN_bm G4_PIN(TRIGGER_PIN)

static void trigger_disable(void);
static void trigger_enable(void);
static void trigger_pressed(void);
static void trigger_tick(TimerHandle_t);
static void trigger_task(void *);

static TimerHandle_t trigger_timer;
static TaskHandle_t trigger_task_handle;

void trigger_init(void) {
	TRIGGER_PORT.DIRCLR = _TRIGPIN_bm;
	TRIGGER_PORT._TRIGPINCTRL = PORT_OPC_PULLUP_gc | PORT_ISC_LEVEL_gc;

	// TODO period.
	trigger_timer = xTimerCreate("trigger", configTICK_RATE_HZ/2, 0, NULL, trigger_tick);

	xTaskCreate(trigger_task, "trigger", 128, NULL, tskIDLE_PRIORITY + 2, &trigger_task_handle);

	//configure interrupts
	TRIGGER_PORT.INT0MASK = _TRIGPIN_bm;
	trigger_enable();
}

static void trigger_task(void *params) {
	while (1) {
		// The only notification this task receices.
		// TODO: combine this with IR TX task.
		if (xTaskNotifyWait(0, 0, NULL, portMAX_DELAY) == pdTRUE)
			trigger_pressed();
	}
}

static void trigger_pressed(void) {
	xTimerStart(trigger_timer, 0);
	mpc_send_cmd(MPC_ADDR_MASTER, MPC_CMD_IR_TX);
}

static void trigger_enable(void) {
	TRIGGER_PORT.INTCTRL |= PORT_INT0LVL_MED_gc;
}
static void trigger_disable(void) {
	TRIGGER_PORT.INTCTRL &= ~PORT_INT0LVL_MED_gc;
}

ISR(PORTA_INT0_vect) {
	if (!(TRIGGER_PORT.IN & _TRIGPIN_bm) /*&&  trigger_enabled()*/) {
		trigger_disable();
		xTaskNotifyFromISR(trigger_task_handle, 0, eNoAction, NULL);
	}
}

void trigger_tick(TimerHandle_t _) {
	trigger_enable();
}
