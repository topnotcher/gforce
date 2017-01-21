#include <saml21/io.h>

#ifndef SERCOM_H
#define SERCOM_H
int sercom_get_index(const Sercom *);
void sercom_register_handler(int, void (*)(void *), void *);
void sercom_enable_irq(int);
void sercom_disable_irq(int);
void sercom_enable_pm(int);
void sercom_set_gclk_core(int, int);
void sercom_set_gclk_slow(int, int);
#endif
