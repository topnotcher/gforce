#include <stdint.h>

#ifndef DISPLAY_H
#define DISPLAY_H

void display_init(void);
void display_send(const uint8_t cmd, uint8_t * data, const uint8_t size);

#endif
