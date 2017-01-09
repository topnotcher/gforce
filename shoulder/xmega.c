#include <avr/io.h>
#include <avr/interrupt.h>

#include <mpc.h>
#include <mpctwi.h>
#include <leds.h>
#include <buzz.h>
#include <irrx.h>
#include <diag.h>

#include <leds.h>
#include <drivers/xmega/led_drivers.h>
#include <drivers/xmega/clock.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "main.h"

static led_spi_dev *led_spi;

system_init_func(system_board_init) {
	sysclk_set_internal_32mhz();

	// Prescale system clock /2
	uint8_t val = CLK.PSCTRL & ~CLK_PSADIV_gm;
	val |= CLK_PSADIV2_bm;

	CCP = CCP_IOREG_gc;
	CLK.PSCTRL = val;

	_Static_assert(configCPU_CLOCK_HZ == 16000000, "configCPU_CLOCK_HZ != 32MHz");
}

system_init_func(system_configure_interrupts) {
	PMIC.CTRL |= PMIC_LOLVLEN_bm | PMIC_MEDLVLEN_bm | PMIC_HILVLEN_bm;
}

system_init_func(system_software_init) {
	mpc_init();
	buzz_init();
	diag_init();

	led_init();
	irrx_init();
}

void mpc_register_drivers(void) {
	mpc_register_driver(mpctwi_init());
}

led_spi_dev *led_init_driver(void) {
	return (led_spi = led_usart_init(&USARTC1, 0));
}
