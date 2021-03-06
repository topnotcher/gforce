#include <stdint.h>

#include <avr/io.h>

#include <malloc.h>
#include <leds.h>
#include <drivers/xmega/spi.h>

typedef struct {
	led_spi_dev controller;

	const uint8_t *data;
	uint8_t data_size;
	uint8_t bytes_written;

	volatile uint8_t *out;
} led_spi_driver;

static void led_spi_load(const led_spi_dev *const, const uint8_t *const, const uint8_t);
static void led_spi_write(const led_spi_dev *const);

led_spi_dev *led_spi_init(SPI_t *const spi) {
	struct spi_port_desc port_info = spi_port_info(spi);

	uint8_t sclk_bm = 1 << port_info.sck_pin;
	uint8_t ss_bm = 1 << port_info.ss_pin;
	uint8_t sout_bm = 1 << port_info.mosi_pin;
	led_spi_driver *dev;

	// SS is required even though we're not using it. xmegaA, p226.
	port_info.port->DIRSET = sclk_bm | sout_bm | ss_bm;
	port_info.port->OUTSET = sclk_bm | sout_bm;
	port_info.port->OUTCLR = ss_bm;

	// NOTE: the controller supports up to 20Mhz. The fastest hardware can do
	// is 16MHz, but without DMA we cannot use 16MHz reliably. Data is latched
	// into the controller's shift regsiter following an 8 clock cycle pause on
	// the SCLK line - we can easily reach this accidentally with higher speeds
	// on the SCLK line (except when using DMA ;)
	spi->CTRL = SPI_ENABLE_bm | SPI_MASTER_bm | SPI_PRESCALER_DIV128_gc;
	spi->INTCTRL = SPI_INTLVL_LO_gc;

	dev = smalloc(sizeof(*dev));
	if (dev) {
		dev->controller.dev = dev;

		dev->controller.write = led_spi_write;
		dev->controller.load = led_spi_load;

		dev->out = &spi->DATA;

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
	// This just writes a junk byte out. If we write a real byte and there's a
	// write collision (i.e. the module is busy sending data already, then we
	// need to restart the transfer).
	led_spi_driver *dev = controller->dev;
	*dev->out = 0xFF;
	dev->bytes_written = 0;
}

void led_spi_tx_isr(const led_spi_dev *const controller) {
	led_spi_driver *dev = controller->dev;

	if (dev->bytes_written < dev->data_size)
		*dev->out = dev->data[dev->bytes_written++];
}
