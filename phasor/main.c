#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <stdlib.h>
#include <string.h>

/**
 * G4 common includes.
 */
#include <g4config.h>
#include <leds.h>
#include <irrx.h>
#include <tasks.h>
#include <mpc.h>
#include "mpcphasor.h"
#include <timer.h>
#include <util.h>
#include <diag.h>

#include "irtx.h"
#include "trigger.h"

#include "FreeRTOS.h"
#include "task.h"

static void ir_cmd_tx(const mpc_pkt *const pkt);

int main(void) {
	sysclk_set_internal_32mhz();

	trigger_init();
	mpc_init();
	mpc_register_driver(mpc_phasor_init());
	init_timers();
	irtx_init();
	tasks_init();
	diag_init();
	mpc_register_cmd(MPC_CMD_IR_TX, ir_cmd_tx);

	PMIC.CTRL |= PMIC_MEDLVLEN_bm | PMIC_LOLVLEN_bm | PMIC_HILVLEN_bm;

	led_init();
	irrx_init();

	vTaskStartScheduler();

	return 0;
}

static void ir_cmd_tx(const mpc_pkt *const pkt) {
	irtx_send((const irtx_pkt *)pkt->data);
}
