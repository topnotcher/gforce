#include <avr/io.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <avr/io.h>
#include <avr/interrupt.h>

#include <g4config.h>
#include "config.h"

/**
 * G4 common includes.
 */
#include <mpc.h>
#include <mpctwi.h>
#include <leds.h>
#include <buzz.h>
#include <irrx.h>
#include <diag.h>
#include <settings.h>

#include <drivers/xmega/clock.h>
#include <drivers/xmega/twi/master.h>
#include <drivers/xmega/twi/slave.h>
#include <drivers/xmega/led_drivers.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static led_spi_dev *led_controller;

int main(void) {
	sysclk_set_internal_32mhz();

	mpc_init();
	settings_init();
	buzz_init();
	diag_init();

	PMIC.CTRL |= PMIC_LOLVLEN_bm | PMIC_MEDLVLEN_bm | /*PMIC.CTRL |*/ PMIC_HILVLEN_bm;

	led_init();
	irrx_init();

	vTaskStartScheduler();

	return 0;
}

void mpc_register_drivers(void) {
	twi_slave_t *twis = twi_slave_init(&MPC_TWI, MPC_ADDR_BOARD, MPC_TWI_ADDRMASK);
	twi_master_t *twim = twi_master_init(&MPC_TWI, MPC_TWI_BAUD);

	mpc_register_driver(mpctwi_init(twim, twis, MPC_ADDR_BOARD, MPC_ADDR_MASTER));
}

led_spi_dev *led_init_driver(void) {
	led_controller = led_spi_init(&SPIC);

	return led_controller;
}

ISR(SPIC_INT_vect) {
	led_spi_tx_isr(led_controller);
}
