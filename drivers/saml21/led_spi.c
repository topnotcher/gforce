#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <saml21/io.h>
#include <malloc.h>

#include <drivers/saml21/led_spi.h>
#include <drivers/saml21/sercom.h>

#include <leds.h>

typedef struct {
	const uint8_t *data;
	Sercom *sercom;

	led_spi_dev controller;

	uint8_t data_size;
	uint8_t bytes_written;
} led_spi_driver;

static void led_spi_load(const led_spi_dev *, const uint8_t *, const uint8_t);
static void led_spi_write(const led_spi_dev *);
static void led_spi_handler(void *);
static inline void led_spi_tx_isr(led_spi_driver *) __attribute__((always_inline));

led_spi_dev *led_spi_init(Sercom *sercom, uint8_t dopo) {
	int sercom_index = sercom_get_index(sercom);
	led_spi_driver *dev = smalloc(sizeof *dev);

	if (sercom_index >= 0 && dev != NULL) {
		memset(dev, 0, sizeof *dev);

		dev->controller.dev = dev;
		dev->controller.write = led_spi_write;
		dev->controller.load = led_spi_load;
		dev->sercom = sercom;

		sercom_enable_pm(sercom_index);
		sercom_set_gclk_core(sercom_index, GCLK_PCHCTRL_GEN_GCLK0);
		sercom_set_gclk_slow(sercom_index, GCLK_PCHCTRL_GEN_GCLK0);

		sercom->SPI.CTRLA.reg &= ~SERCOM_SPI_CTRLA_ENABLE;
		while (sercom->SPI.SYNCBUSY.reg & SERCOM_SPI_SYNCBUSY_ENABLE);

		uint32_t ctrla = SERCOM_SPI_CTRLA_MODE(0x03);
		uint32_t ctrlb = 0; 

		ctrla |= SERCOM_SPI_CTRLA_DOPO(dopo);
		//ctrla |= SERCOM_SPI_CTRLA_CPOL;

		sercom->SPI.CTRLA.reg = ctrla;
		sercom->SPI.CTRLB.reg = ctrlb;
		sercom->SPI.BAUD.reg = 15; // TODO. 16MHz/(2 * (15 + 1)) = 500KHz

		sercom->SPI.CTRLA.reg |= SERCOM_SPI_CTRLA_ENABLE;
		while (sercom->SPI.SYNCBUSY.reg & SERCOM_SPI_SYNCBUSY_ENABLE);

		sercom->SPI.INTENSET.reg = SERCOM_SPI_INTENSET_ERROR;

		sercom_register_handler(sercom_index, led_spi_handler, dev);
		sercom_enable_irq(sercom_index);

		return &dev->controller;
	} else {
		return NULL;
	}
}

static void led_spi_load(const led_spi_dev *const controller, const uint8_t *const data, const uint8_t size) {
	led_spi_driver *dev = controller->dev;
	dev->data = data;
	dev->data_size = size;
}

static void led_spi_write(const led_spi_dev *const controller) {
	led_spi_driver *dev = controller->dev;
	dev->bytes_written = 0;
	dev->sercom->SPI.INTENSET.reg = SERCOM_SPI_INTENSET_DRE;
}

static inline void led_spi_tx_isr(led_spi_driver *const dev) {
	if (dev->bytes_written < dev->data_size)
		dev->sercom->SPI.DATA.reg = dev->data[dev->bytes_written++];
	else
		dev->sercom->SPI.INTENCLR.reg = SERCOM_SPI_INTENCLR_DRE;
}

static void led_spi_handler(void *_isr_ins) {
	led_spi_driver *dev = _isr_ins;

	if (dev->sercom->SPI.INTFLAG.reg & SERCOM_SPI_INTFLAG_DRE) {
		led_spi_tx_isr(dev);

	} else if (dev->sercom->SPI.INTFLAG.reg & SERCOM_SPI_INTFLAG_ERROR) {
		// TODO
		dev->sercom->SPI.INTFLAG.reg = SERCOM_SPI_INTFLAG_ERROR;
	}
}
