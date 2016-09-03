#include <stdint.h>

#include "leds.h"
#include "g4config.h"

#ifndef TLC59711_H
#define TLC59711_H

#define TLC59711_CMD_WRITE 0x25
#define TLC59711_BRIGHTNESS_MAX 0x7F

enum {
	TLC59711_OUTMG,
	TLC59711_EXTGCK,
	TLC59711_TMGRST,
	TLC59711_DSPRPT,
	TLC59711_BLANK,
} tlc59711_bit;

enum {
	TLC59711_RED = 1,
	TLC59711_GREEN = 2,
	TLC59711_BLUE = 4,
};

typedef struct {
	uint16_t blue;
	uint16_t green;
	uint16_t red;
} __attribute__((packed)) led_color;

/**
 * Note: we need to send the 224-bits of data to the controller MSB->LSB. For
 * the LED color values this is trivial: we store them big endian then write
 * out the bytes in order using the SPI driver to send in MSB -> LSB order.
 *
 * For the header bytes, this is more difficult...
 * [6 bits of cmd][1 bit outmg][1 bit extgck][1 bit tmgrst][1 bit dsprpt][1 bit blank]
 * [1 bit blank][7 bits blue brightness][7 bits green brightness][7 bits red brightness]
 *
 * The first 11 bits are command/config. The next 7 are blue birghtness, which
 * crosses a byte boundary: we must write the 5 MSBs of the brightness to one
 * byte and then the next two bits in bits 7, 6 of the next byte.
 */
typedef struct {
	uint8_t header[4];
	led_color leds[4];
} __attribute__((packed)) led_control;

typedef struct {
	uint8_t controllers;
	const led_spi_dev *spi;

	led_control __data[];
} tlc59711_dev;

void tlc59711_set_color(tlc59711_dev *dev, uint8_t led, uint16_t red, uint16_t green, uint16_t blue);

static inline void tlc59711_write(const tlc59711_dev *const dev) {
	dev->spi->write(dev->spi);
}

tlc59711_dev *tlc59711_init(led_spi_dev *const, const uint8_t);
void tlc59711_set_brightness(tlc59711_dev *, const uint8_t, const uint8_t);
uint8_t tlc59711_get_brightness(tlc59711_dev *, const uint8_t);
void tlc59711_set(tlc59711_dev *, const uint8_t, const uint8_t);
void tlc59711_controller_set(tlc59711_dev *, const uint8_t, const uint8_t, const uint8_t);

#endif
