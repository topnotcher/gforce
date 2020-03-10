#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <sam.h>
#include <malloc.h>

#include <drivers/sam/led_spi.h>
#include <drivers/sam/sercom.h>
#include <drivers/sam/dma.h>

#include <freertos/FreeRTOS.h>

#include <leds.h>

typedef struct {
	sercom_t sercom;
	led_spi_dev controller;
	uint8_t dma_chan;
} led_spi_driver;

static void led_spi_load(const led_spi_dev *, const uint8_t *, const uint8_t);
static void led_spi_write(const led_spi_dev *);
static void configure_dma(led_spi_driver *, const sercom_t *const);

led_spi_dev *led_spi_init(const int sercom_index, uint8_t dopo, uint8_t gclk_id) {
	led_spi_driver *dev = smalloc(sizeof *dev);

	if (dev != NULL) {
		memset(dev, 0, sizeof(*dev));
	}

	if (dev != NULL && sercom_init(sercom_index, &dev->sercom)) {
		Sercom *hw = dev->sercom.hw;

		dev->controller.dev = dev;
		dev->controller.write = led_spi_write;
		dev->controller.load = led_spi_load;
		configure_dma(dev, &dev->sercom);

		sercom_set_gclk_core(&dev->sercom, gclk_id);
		sercom_set_gclk_slow(&dev->sercom, gclk_id);

		hw->SPI.CTRLA.reg = SERCOM_SPI_CTRLA_SWRST;
		while (hw->SPI.SYNCBUSY.reg & SERCOM_SPI_SYNCBUSY_SWRST);

		uint32_t ctrla = SERCOM_SPI_CTRLA_MODE(0x03);
		uint32_t ctrlb = 0; 

		ctrla |= SERCOM_SPI_CTRLA_DOPO(dopo);

		hw->SPI.CTRLA.reg = ctrla;
		hw->SPI.CTRLB.reg = ctrlb;

		// Max sclk for LED controller is 20MHz 
		// CPU_CLOCK_HZ/(2 * (BAUD + 1)) = ...
		hw->SPI.BAUD.reg = (configCPU_CLOCK_HZ > 40000000UL) ? 1 : 0;

		hw->SPI.CTRLA.reg |= SERCOM_SPI_CTRLA_ENABLE;
		while (hw->SPI.SYNCBUSY.reg & SERCOM_SPI_SYNCBUSY_ENABLE);

		return &dev->controller;
	} else {
		return NULL;
	}
}

static void led_spi_load(const led_spi_dev *const controller, const uint8_t *const data, const uint8_t size) {
	led_spi_driver *dev = controller->dev;
	DmacDescriptor *desc = dma_channel_desc(dev->dma_chan);

	// See the SRCADDR description in the DS. This is how it says to calculate
	// SRCADDR when SRCINC=1.
	desc->SRCADDR.reg = (uint32_t)data + size;
	desc->BTCNT.reg = size;
}

static void led_spi_write(const led_spi_dev *const controller) {
	led_spi_driver *dev = controller->dev; 
	dma_start_transfer(dev->dma_chan);
}

static void configure_dma(led_spi_driver *const dev, const sercom_t *const sercom) {
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
		desc->DSTADDR.reg = (uint32_t)&sercom->hw->SPI.DATA.reg;

		// TODO - add a dma_function() for this. Should only access channel
		// registers in a critical section (or with context switches disabled)...
#if defined(__SAML21E17B__)
		//note: assumed this is called from a critical section...
		DMAC->CHID.reg = dev->dma_chan;
		DMAC->CHCTRLB.reg =
			DMAC_CHCTRLB_TRIGSRC(sercom_dma_tx_trigsrc(sercom)) |
			DMAC_CHCTRLB_TRIGACT_BEAT;
#elif defined(__SAMD51N20A__)
		// TODO HACK: this whole thing
		DMAC->Channel[dma_chan].CHCTRLA.reg =
			DMAC_CHCTRLA_TRIGSRC(sercom_dma_tx_trigsrc(sercom)) |
			DMAC_CHCTRLA_TRIGACT_BURST; // TODO HACK: -- just changed to compile
#else
	#error "Unsupported part!"
#endif
	}
}
