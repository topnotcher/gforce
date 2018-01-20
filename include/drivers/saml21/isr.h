#include <stdint.h>

#include <saml21/io.h>

#ifndef SAML21_ISR_H
#define SAML21_ISR_H
void nvic_register_isr(const uint32_t, void (*const)(void));
static inline __attribute__((always_inline)) int get_active_irqn(void) {
	const uint32_t ipsr = __get_IPSR();

	// Per the Cortex M0+/M4 User guide, 16 = IRQ0.
	return (ipsr & 0x1FF) - 16;
}
#endif
