#include <stdint.h>
#include <stdlib.h>

#include <avr/io.h>

#include "malloc.h"
#include "leds.h"

typedef struct {
	led_controller controller;

	const uint8_t *data;
	uint8_t data_size;
	uint8_t bytes_written;

	USART_t *usart;
} led_usart_dev;


static void led_usart_load(const led_controller *const, const uint8_t *const, const uint8_t);
static void led_usart_write(const led_controller *const);
static void led_usart_tx_isr(const led_controller *const);

static void led_usart_dma_load(const led_controller *const, const uint8_t *const, const uint8_t);
static void led_usart_dma_write(const led_controller *const controller);
static uint8_t led_usart_dma_get_trigsrc(const USART_t *const usart);

led_controller *led_usart_init(
	USART_t *const usart, PORT_t *const port, const uint8_t sclk_pin,
	const uint8_t sout_pin, const int8_t dma_ch)

{
	uint16_t bsel;
	led_controller *controller = NULL;

	uint8_t sclk_bm = 1 << sclk_pin;
	uint8_t sout_bm = 1 << sout_pin;

	// yeah, I know ;)...
	volatile uint8_t *port_pinctrl = &port->PIN0CTRL;

	if (dma_ch >= 0) {
		DMA_CH_t *dma_channels = &DMA.CH0;

		bsel = 1;

		DMA.CTRL |= DMA_ENABLE_bm;

		// DMA source address is incremented during transaction and reset at the
		// end of every transaction; dest address is fixed.
		dma_channels[dma_ch].ADDRCTRL = DMA_CH_SRCRELOAD_TRANSACTION_gc | DMA_CH_DESTRELOAD_TRANSACTION_gc | DMA_CH_SRCDIR_INC_gc | DMA_CH_DESTDIR_FIXED_gc;
		dma_channels[dma_ch].TRIGSRC = led_usart_dma_get_trigsrc(usart);
		dma_channels[dma_ch].CTRLA = DMA_CH_SINGLE_bm;

		#pragma GCC diagnostic push
		#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
		dma_channels[dma_ch].DESTADDR0 = ((uint32_t)&usart->DATA) & 0xFF;
		dma_channels[dma_ch].DESTADDR1 = (((uint32_t)&usart->DATA) >> 8) & 0xFF;
		dma_channels[dma_ch].DESTADDR2 = (((uint32_t)&usart->DATA) >> 16) & 0xFF;
		#pragma GCC diagnostic pop

		if ((controller = smalloc(sizeof *controller))) {
			controller->write = led_usart_dma_write;
			controller->load = led_usart_dma_load;
			controller->dev = &dma_channels[dma_ch];
		}

	} else {
		// Without DMA we might prematurely latch the data
		bsel = 8;

		led_usart_dev *dev;
		if ((dev = smalloc(sizeof *dev))) {
			// whoa it's a cirlce!
			controller = &dev->controller;
			controller->dev = dev;
			dev->usart = usart;

			controller->load = led_usart_load;
			controller->write = led_usart_write;
			controller->tx_isr = led_usart_tx_isr;
		}
	}

	port->OUTSET = sout_bm | sclk_bm;
	port->DIRSET = sclk_bm | sout_bm;
	port_pinctrl[sclk_pin] = PORT_INVEN_bm;

	usart->BAUDCTRLA = (uint8_t)(bsel & 0x00FF);
	usart->BAUDCTRLB = (uint8_t)((bsel >> 8) & 0x000F);
	usart->CTRLC |= USART_CMODE_MSPI_gc;
	usart->CTRLB |= USART_TXEN_bm;

	return controller;
}

static uint8_t led_usart_dma_get_trigsrc(const USART_t *const usart) {
#ifdef USARTC0
	if (usart == &USARTC0)
		return DMA_CH_TRIGSRC_USARTC0_DRE_gc;
#endif
#ifdef USARTC1
	if (usart == &USARTC1)
		return DMA_CH_TRIGSRC_USARTC1_DRE_gc;
#endif
#ifdef USARTD0
	if (usart == &USARTD0)
		return DMA_CH_TRIGSRC_USARTD0_DRE_gc;
#endif
#ifdef USARTD1
	if (usart == &USARTD1)
		return DMA_CH_TRIGSRC_USARTD1_DRE_gc;
#endif
#ifdef USARTE0
	if (usart == &USARTE0)
		return DMA_CH_TRIGSRC_USARTE0_DRE_gc;
#endif
#ifdef USARTE1
	if (usart == &USARTE1)
		return DMA_CH_TRIGSRC_USARTE1_DRE_gc
#endif
#ifdef USARTF0
	if (usart == &USARTF0)
		return DMA_CH_TRIGSRC_USARTF0_DRE_gc;
#endif
#ifdef USARTF1
	if (usart == &USARTF1)
		return DMA_CH_TRIGSRC_USARTF1_DRE_gc;
#endif
		return 0;
}

static void led_usart_load(const led_controller *const controller, const uint8_t *const data, const uint8_t size) {
	led_usart_dev *dev = controller->dev;
	dev->data = data;
	dev->data_size = size;
}

static void led_usart_write(const led_controller *const controller) {
	led_usart_dev *dev = controller->dev;

	dev->usart->CTRLA &= ~USART_DREINTLVL_LO_gc;
	dev->bytes_written = 0;
	dev->usart->CTRLA |= USART_DREINTLVL_LO_gc;
}

static void led_usart_tx_isr(const led_controller *const controller) {
	led_usart_dev *dev = controller->dev;

	dev->usart->DATA = dev->data[dev->bytes_written++];

	if (dev->bytes_written >= dev->data_size)
		dev->usart->CTRLA &= ~USART_DREINTLVL_LO_gc;
}

static void led_usart_dma_load(const led_controller *const controller, const uint8_t *const data, const uint8_t size) {
	DMA_CH_t *dma = controller->dev;

	// TODO: define dev
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
	dma->SRCADDR0 = ((uint32_t)data) & 0xFF;
	dma->SRCADDR1 = (((uint32_t)data) >> 8) & 0xFF;
	dma->SRCADDR2 = (((uint32_t)data) >> 16) & 0xFF;
	#pragma GCC diagnostic pop

	dma->TRFCNTL = size & 0xFF;
	dma->TRFCNTH = 0;

	dma->CTRLA |= DMA_CH_ENABLE_bm;
}

static void led_usart_dma_write(const led_controller *const controller) {
	((DMA_CH_t*)(controller->dev))->CTRLA |= DMA_CH_ENABLE_bm;
}
