#include "leds.h"

#ifndef XMEGA_LED_DRIVERS_H
#define XMEGA_LED_DRIVERS_H

led_spi_dev *led_spi_init(SPI_t *const);
led_spi_dev *led_usart_init(USART_t *const, const uint8_t);

#endif
