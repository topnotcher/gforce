#include <stdint.h>
#include <stdbool.h>
#include <sam.h>

#ifndef PORT_PINMUX_H
#define PORT_PINMUX_H

#define _port_group(pin) (PORT[0].Group[pin / 32])
#define _pin_cfg(pin) (_port_group(pin).PINCFG[pin % 32].reg)
#define _pin_mux(pin) (_port_group(pin).PMUX[(pin % 32) / 2].reg)

void port_pinmux(uint32_t);

static __attribute__((always_inline)) inline void pin_set_config(const uint16_t pin, const uint32_t config) {
	_pin_cfg(pin) = (config & (PORT_PINCFG_MASK & ~PORT_PINCFG_PMUXEN)) | (_pin_cfg(pin) & PORT_PINCFG_PMUXEN);
}

static __attribute__((always_inline)) inline void pin_enable_pinmux(const uint16_t pin) {
	_pin_cfg(pin) |= PORT_PINCFG_PMUXEN;
}

static __attribute__((always_inline)) inline void pin_disable_pinmux(const uint16_t pin) {
	_pin_cfg(pin) &= ~PORT_PINCFG_PMUXEN;
}

static __attribute__((always_inline)) inline void pin_set_output(const uint16_t pin) {
	_port_group(pin).DIRSET.reg = 1 << (pin % 32);
}

static __attribute__((always_inline)) inline void pin_set_input(const uint16_t pin) {
	_port_group(pin).DIRCLR.reg = 1 << (pin % 32);
}

static __attribute__((always_inline)) inline void pin_set(const uint16_t pin, const bool value) {
	if (value)
		_port_group(pin).OUTSET.reg = 1 << (pin % 32);
	else
		_port_group(pin).OUTCLR.reg = 1 << (pin % 32);
}

static __attribute__((always_inline)) inline bool pin_get(const uint16_t pin) {
	return (_port_group(pin).IN.reg & 1 << (pin % 32)) != 0;
}

#endif
