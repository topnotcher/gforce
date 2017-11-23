#include <avr/io.h>
#include <avr/interrupt.h>

/**
 * G4 common includes.
 */
#include <g4config.h>
#include <leds.h>
#include <irrx.h>
#include <mpc.h>
#include "mpcphasor.h"
#include <diag.h>
#include <settings.h>

#include <drivers/xmega/clock.h>
#include <drivers/xmega/led_drivers.h>

#include "irtx.h"
#include "trigger.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static void ir_cmd_tx(const mpc_pkt *const pkt);
static led_spi_dev *led_controller;

int main(void) {
	sysclk_set_internal_32mhz();

	mpc_init();
	settings_init();
	trigger_init();
	irtx_init();
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

void mpc_register_drivers(void) {
	mpc_register_driver(mpc_phasor_init());
}

led_spi_dev *led_init_driver(void) {
	led_controller = led_spi_init(&SPID);

	return led_controller;
}

ISR(SPID_INT_vect) {
	led_spi_tx_isr(led_controller);
}
