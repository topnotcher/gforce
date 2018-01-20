#include <stdint.h>

#ifndef SAML21_ISR_H
#define SAML21_ISR_H
void nvic_register_isr(const uint32_t, void (*const)(void));
#endif
