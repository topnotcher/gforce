#include <stdint.h>
#include <util.h>
#include <avr/io.h>

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

void crc(uint8_t *const shift, uint8_t byte, const uint8_t poly) {
	uint8_t fb;

	for (uint8_t i = 0; i < 8; i++) {
		fb = (*shift ^ byte) & 0x01;

		if (fb)
			*shift = 0x80 | (((*shift) ^ poly) >> 1);
		else
			*shift >>= 1;
		byte >>= 1;
	}
}
