#include <stdint.h>

#ifndef DISPLAY_H
#define DISPLAY_H

void display_init(void);
void display_send(const uint8_t cmd, const uint8_t size, uint8_t * data);
void display_write(char *);
#endif
