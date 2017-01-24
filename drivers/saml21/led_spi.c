#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <saml21/io.h>
#include <malloc.h>

#include <drivers/saml21/led_spi.h>
#include <drivers/saml21/sercom.h>
#include <drivers/saml21/dma.h>

#include <freertos/FreeRTOS.h>

#include <leds.h>

typedef struct {
	led_spi_dev controller;
	uint8_t dma_chan;
} led_spi_driver;

static void led_spi_load(const led_spi_dev *, const uint8_t *, const uint8_t);
static void led_spi_write(const led_spi_dev *);
static void configure_dma(led_spi_driver *,  const Sercom *);

led_spi_dev *led_spi_init(Sercom *sercom, uint8_t dopo) {
	int sercom_index = sercom_get_index(sercom);
	led_spi_driver *dev = smalloc(sizeof *dev);

	if (sercom_index >= 0 && dev != NULL) {
		memset(dev, 0, sizeof *dev);

		dev->controller.dev = dev;
		dev->controller.write = led_spi_write;
		dev->controller.load = led_spi_load;
		configure_dma(dev, sercom);

		sercom_enable_pm(sercom_index);
		sercom_set_gclk_core(sercom_index, GCLK_PCHCTRL_GEN_GCLK0);
		sercom_set_gclk_slow(sercom_index, GCLK_PCHCTRL_GEN_GCLK0);

		sercom->SPI.CTRLA.reg &= ~SERCOM_SPI_CTRLA_ENABLE;
		while (sercom->SPI.SYNCBUSY.reg & SERCOM_SPI_SYNCBUSY_ENABLE);

		uint32_t ctrla = SERCOM_SPI_CTRLA_MODE(0x03);
		uint32_t ctrlb = 0; 

		ctrla |= SERCOM_SPI_CTRLA_DOPO(dopo);

		sercom->SPI.CTRLA.reg = ctrla;
		sercom->SPI.CTRLB.reg = ctrlb;

		// Max sclk for LED controller is 20MHz 
		// CPU_CLOCK_HZ/(2 * (BAUD + 1)) = ...
		sercom->SPI.BAUD.reg = (configCPU_CLOCK_HZ > 40000000UL) ? 1 : 0;

		sercom->SPI.CTRLA.reg |= SERCOM_SPI_CTRLA_ENABLE;
		while (sercom->SPI.SYNCBUSY.reg & SERCOM_SPI_SYNCBUSY_ENABLE);

		return &dev->controller;
	} else {
		return NULL;
	}
}

static void led_spi_load(const led_spi_dev *const controller, const uint8_t *const data, const uint8_t size) {
	led_spi_driver *dev = controller->dev;
	DmacDescriptor *desc = dma_channel_desc(dev->dma_chan);

	desc->SRCADDR.reg = (uint32_t)data + size;
	desc->BTCNT.reg = size;
}

static void led_spi_write(const led_spi_dev *const controller) {
	led_spi_driver *dev = controller->dev; 

	dma_start_transfer(dev->dma_chan);
}

static void configure_dma(led_spi_driver *const dev, const Sercom *const sercom) {
	int dma_chan = dma_channel_alloc();

	if (dma_chan >= 0) {
		dev->dma_chan = dma_chan;
		DmacDescriptor *desc = dma_channel_desc(dev->dma_chan);
		desc->BTCTRL.reg =
			DMAC_BTCTRL_BEATSIZE_BYTE |
			DMAC_BTCTRL_SRCINC |
			DMAC_BTCTRL_BLOCKACT_NOACT; // what is suspend?

		desc->BTCNT.reg = 0;
		desc->DESCADDR.reg = 0;
		desc->DSTADDR.reg = (uint32_t)&sercom->SPI.DATA.reg;

		// TODO - add a dma_function() for this. Should only access channel
		// registers in a critical section (or with context switches disabled)...
		DMAC->CHID.reg = dev->dma_chan;
		DMAC->CHCTRLB.reg =
			DMAC_CHCTRLB_TRIGSRC(sercom_dma_tx_trigsrc(sercom_get_index(sercom))) |
			DMAC_CHCTRLB_TRIGACT_BEAT;
	}
}
