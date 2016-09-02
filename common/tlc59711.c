#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "leds.h"
#include "malloc.h"
#include "tlc59711.h"

tlc59711_dev *tlc59711_init(led_spi_dev *const spi, const uint8_t controllers) {
	tlc59711_dev *tlc;
	size_t data_size = sizeof(tlc->__data[0]) * controllers;

	if ((tlc = smalloc((sizeof *tlc) + data_size))) {
		tlc->spi = spi;
		tlc->controllers = controllers;

		memset(tlc->__data, 0, data_size);

		for (uint8_t i = 0; i < tlc->controllers; ++i) {
			tlc->__data[i].header[0] = (TLC59711_CMD_WRITE & 0x3F) << 2;

			// noise reduction - see the datasheet
			tlc59711_controller_set(tlc, i, TLC59711_OUTMG, i & 1);

			tlc59711_controller_set(tlc, i, TLC59711_EXTGCK, 0);
			tlc59711_controller_set(tlc, i, TLC59711_TMGRST, 0);
			tlc59711_controller_set(tlc, i, TLC59711_DSPRPT, 1);
			tlc59711_controller_set(tlc, i, TLC59711_BLANK, 0);
		}

		// default to 1/2 brightness
		tlc59711_set_brightness(tlc, TLC59711_RED | TLC59711_GREEN | TLC59711_BLUE, 0x40);

		tlc->spi->load(tlc->spi, (uint8_t*)tlc->__data, data_size);
	}

	return tlc;
}


void tlc59711_set_brightness(tlc59711_dev *dev, const uint8_t color, const uint8_t value) {
	for (uint8_t i = 0; i < dev->controllers; ++i) {
		uint8_t *header = dev->__data[i].header;

		if (color & TLC59711_BLUE) {
			header[1] &= ~0x1F;
			header[2] &= ~0xC0;

			header[1] |= (value >> 2) & 0x1F;
			header[2] |= (value << 6) & 0xC0;
		}

		if (color & TLC59711_GREEN) {
			header[2] &= ~0x3F;
			header[3] &= ~0x80;

			header[2] |= (value >> 1) & 0x3F;
			header[3] |= (value << 7) & 0x80;
		}

		if (color & TLC59711_RED) {
			header[3] &= ~0x7F;

			header[3] |= value & 0x7F;
		}
	}
}

void tlc59711_set(tlc59711_dev *dev, const uint8_t bit, const uint8_t value) {
	for (uint8_t i = 0; i < dev->controllers; ++i)
		tlc59711_controller_set(dev, i, bit, value);
}

void tlc59711_controller_set(tlc59711_dev *dev, const uint8_t controller, const uint8_t bit, const uint8_t value) {
	uint8_t *header;
	uint8_t byte_offset;
	uint8_t bit_offset;

	switch (bit) {
	case TLC59711_OUTMG:
		byte_offset = 0;
		bit_offset = 1;
		break;

	case TLC59711_EXTGCK:
		byte_offset = 0;
		bit_offset = 0;
		break;

	case TLC59711_TMGRST:
		byte_offset = 1;
		bit_offset = 7;
		break;

	case TLC59711_DSPRPT:
		byte_offset = 1;
		bit_offset = 6;
		break;

	case TLC59711_BLANK:
		byte_offset = 1;
		bit_offset = 5;
		break;

	default:
		return;
	}

	header = dev->__data[controller].header;
	header[byte_offset] = (header[byte_offset] & ~(1 << bit_offset)) | (value ? (1 << bit_offset) : 0);
}

void tlc59711_set_color(tlc59711_dev *dev, uint8_t led, uint16_t red, uint16_t green, uint16_t blue) {
	uint8_t controller = (led >> 2) & 0x3F;
	uint8_t led_offset = led & 0x03;

	dev->__data[controller].leds[led_offset].red = ((red >> 8) & 0xFF) | (red & 0xFF);
	dev->__data[controller].leds[led_offset].green = ((green >> 8) & 0xFF) | (green & 0xFF);
	dev->__data[controller].leds[led_offset].blue = ((blue >> 8) & 0xFF) | (blue & 0xFF);
}
