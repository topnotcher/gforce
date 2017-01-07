#include <avr/io.h>

#include <drivers/xmega/clock.h>

#define CLKSYS_Enable( _oscSel ) ( OSC.CTRL |= (_oscSel) )
#define CLKSYS_IsReady( _oscSel ) ( OSC.STATUS & (_oscSel) )

void sysclk_set_internal_32mhz(void) {

	// Enable 32 MHz Osc.
	CLKSYS_Enable( OSC_RC32MEN_bm );

	// Wait for 32 MHz Osc. to stabilize
	while (CLKSYS_IsReady( OSC_RC32MRDY_bm ) == 0);

	// enable 32Khz osc
	CLKSYS_Enable( OSC_RC32KEN_bm );

	while (CLKSYS_IsReady( OSC_RC32KRDY_bm ) == 0);

	//Enable 32Mhz DFLL and set the reference clock to the 32Khz osc
    OSC.DFLLCTRL = OSC_RC32MCREF_RC32K_gc;
	DFLLRC32M.CTRL |= DFLL_ENABLE_bm;

	//Unlock seq. to access CLK.CTRL
	CCP = CCP_IOREG_gc;

	// Select 32Mhz internal as sys. clk. 
	CLK.CTRL = CLK_SCLKSEL_RC32M_gc;
}

void clk_enable_rtc_systick_256hz(void) {
	// enable 32Khz osc
	CLKSYS_Enable( OSC_RC32KEN_bm );
	while (CLKSYS_IsReady( OSC_RC32KRDY_bm ) == 0);

	// Lock source to 1024Hz from internal 32Khz osc and enable RTC 
	CLK.RTCCTRL = CLK_RTCSRC_RCOSC_gc /*CLK_RTCSRC_ULP_gc*/ | CLK_RTCEN_bm;
	RTC.CTRL = RTC_PRESCALER_DIV2_gc; // 512 Hz

	/**
	 * RTC must be enabled and SYNCBUSY cleared before writing each of these registers.
	 */
	while (RTC.STATUS & RTC_SYNCBUSY_bm);
	RTC.PER = 1; // 256 Hz

	while (RTC.STATUS & RTC_SYNCBUSY_bm);
	RTC.CNT = 0;

	// Enable overflow interrupt vector. CNT resuts to 0 when CNT == PER
	RTC.INTCTRL |= RTC_OVFINTLVL_HI_gc;
}
