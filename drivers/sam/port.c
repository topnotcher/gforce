#include <drivers/sam/port.h>

#define _port_group(pin) (PORT[0].Group[pin / 32])
#define _pin_cfg(pin) (_port_group(pin).PINCFG[pin % 32].reg)
#define _pin_mux(pin) (_port_group(pin).PMUX[(pin % 32) / 2].reg)


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

	_pin_cfg(pin) |= PORT_PINCFG_PMUXEN;
}
