#include <drivers/sam/port.h>


void port_pinmux(uint32_t mux) {
	uint32_t pin = (mux >> 16) & 0xFFFF;
	uint32_t conf = mux & 0xFFFF;

	if (pin & 1) {
		_pin_mux(pin) &= ~PORT_PMUX_PMUXO_Msk;
		_pin_mux(pin) |= PORT_PMUX_PMUXO(conf);
	} else {
		_pin_mux(pin) &= ~PORT_PMUX_PMUXE_Msk;
		_pin_mux(pin) |= PORT_PMUX_PMUXE(conf);
	}

	pin_enable_pinmux(pin);
}
