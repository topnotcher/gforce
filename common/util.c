#include <stdint.h>
#include <util.h>

void sysclk_set_internal_32mhz(void) {
	// Enable 32 MHz Osc.
	CLKSYS_Enable( OSC_RC32MEN_bm );
	// Wait for 32 MHz Osc. to stabilize
	while (CLKSYS_IsReady( OSC_RC32MRDY_bm ) == 0);
	// was 8  PLL mult. factor ( 2MHz x8 ) and set clk source to PLL.
	OSC_PLLCTRL = 16;

	//enable PLL
	OSC_CTRL = 0x10;

	//wait for PLL clk to be ready
	while (!(OSC_STATUS & 0x10));

	//Unlock seq. to access CLK_CTRL
	CCP = 0xD8;

	// Select PLL as sys. clk. These 2 lines can ONLY go here to engage the PLL ( reverse of what manual A pg 81 says )
	CLK_CTRL = 0x04;

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
