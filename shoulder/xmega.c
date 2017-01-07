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
	_Static_assert(configCPU_CLOCK_HZ == 32000000, "configCPU_CLOCK_HZ != 32MHz");
}

system_init_func(system_configure_os_ticks) {
	_Static_assert(configTICK_RATE_HZ == 256, "configTICK_RATE_HZ != 256");
	clk_enable_rtc_systick_256hz();
}

// RTC - 256Hz as configured above.
ISR(RTC_OVF_vect) {
	xTaskIncrementTick();
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
