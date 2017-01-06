#include <avr/io.h>
#include <avr/interrupt.h>

#include "mpc.h"
#include "mpctwi.h"
#include "leds.h"
#include "buzz.h"
#include "irrx.h"
#include "diag.h"

#include "xmega/clock.h"

#include "FreeRTOS.h"
#include "task.h"

#include "main.h"

bsp_init_func(system_board_init) {
	sysclk_set_internal_32mhz();
	_Static_assert(configCPU_CLOCK_HZ == 32000000, "configCPU_CLOCK_HZ != 32MHz");
}

bsp_init_func(system_configure_os_ticks) {
	_Static_assert(configTICK_RATE_HZ == 256, "configTICK_RATE_HZ != 256");
	clk_enable_rtc_systick_256hz();
}

// RTC - 256Hz as configured above.
ISR(RTC_OVF_vect) {
	xTaskIncrementTick();
}

bsp_init_func(system_configure_interrupts) {
	PMIC.CTRL |= PMIC_LOLVLEN_bm | PMIC_MEDLVLEN_bm | PMIC_HILVLEN_bm;
}

bsp_init_func(system_software_init) {
	mpc_init();
	buzz_init();
	diag_init();

	led_init();
	irrx_init();
}

void mpc_register_drivers(void) {
	mpc_register_driver(mpctwi_init());
}
