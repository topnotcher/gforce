#include <stdint.h>

#ifndef LIGHTS_H
#define LIGHTS_H

void lights_on(void);
void lights_off(void);
void lights_stun(void);
void lights_unstun(void);
void lights_halfstun(void);
void lights_booting(const uint8_t addr);
void lights_countdown(void);

#endif
