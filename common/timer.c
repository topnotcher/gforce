#include <avr/interrupt.h>

#include "timer.h"

#include "FreeRTOS.h"
#include "portmacro.h"
#include "task.h"

/**
 * Initialize the timers: run at boot.
 */
void init_timers(void) {
	CLK.RTCCTRL = CLK_RTCSRC_RCOSC_gc /*CLK_RTCSRC_ULP_gc*/ | CLK_RTCEN_bm;
	RTC.CTRL = RTC_PRESCALER_DIV2_gc; // 512 Hz

	/**
	 * RTC must be enabled and SYNCBUSY cleared before writing each of these registers.
	 */
	while (RTC.STATUS & RTC_SYNCBUSY_bm);
	RTC.PER = 1; // 256 Hz

	while (RTC.STATUS & RTC_SYNCBUSY_bm);
	RTC.CNT = 0;

	TIMER_INTERRUPT_REGISTER |= TIMER_INTERRUPT_ENABLE_BITS;
}

TIMER_RUN {
	xTaskIncrementTick();
}
