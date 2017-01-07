#include <stdint.h>

#include "led_patterns.h"

#ifndef LEDS_H
#define LEDS_H

#define LED_DELAY_TIME 3

struct led_spi_dev_s;
typedef struct led_spi_dev_s {
	// load data to be written into the driver
	// This is separate from write as a stupid optimization for the DMA case.
	void (*load)(const struct led_spi_dev_s *const, const uint8_t *const, const uint8_t);

	// start writing out the actual data
	void (*write)(const struct led_spi_dev_s *const);

	// handle a tx interrupt
	void (*tx_isr)(const struct led_spi_dev_s *const);

	// underlying device driver
	void *dev;
} led_spi_dev;

led_spi_dev *led_init_driver(void);

void led_init(void);
#endif
