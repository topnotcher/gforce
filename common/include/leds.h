#include <stdint.h>

#include "led_patterns.h"

#ifndef LEDS_H
#define LEDS_H

#define LED_DELAY_TIME 13

struct led_controller_s;
typedef struct led_controller_s {
	// load data to be written into the driver
	// This is separate from write as a stupid optimization for the DMA case.
	void (*load)(const struct led_controller_s *const, const uint8_t *const, const uint8_t);

	// start writing out the actual data
	void (*write)(const struct led_controller_s *const);

	// handle a tx interrupt
	void (*tx_isr)(const struct led_controller_s *const);

	// underlying device driver
	void *dev;
} led_controller;

led_controller *led_spi_init(
	SPI_t *const, PORT_t *const, const uint8_t,
	const uint8_t sout_pin, const uint8_t
);

led_controller *led_usart_init(
	USART_t *const, PORT_t *const, const uint8_t,
	const uint8_t, const int8_t
);

void led_init(void);
#endif
