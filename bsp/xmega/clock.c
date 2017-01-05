#include <avr/io.h>

#include "clock.h"

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
