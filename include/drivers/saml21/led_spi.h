#include <stdint.h>

#include <sam.h>
#include <leds.h>

#ifndef SAML21_LED_SPI_H
#define SAML21_LED_SPI_H
led_spi_dev *led_spi_init(Sercom *, uint8_t);
#endif
