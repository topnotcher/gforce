#include <stdint.h>
#include <stdlib.h>

#include <avr/io.h>

#include <malloc.h>
#include <leds.h>
#include <drivers/xmega/uart.h>

typedef struct {
	led_spi_dev controller;

	const uint8_t *data;
	uint8_t data_size;
	uint8_t bytes_written;

	USART_t *usart;
} led_usart_driver;


static void led_usart_load(const led_spi_dev *const, const uint8_t *const, const uint8_t);
static void led_usart_write(const led_spi_dev *const);

static void led_usart_dma_load(const led_spi_dev *const, const uint8_t *const, const uint8_t);
static void led_usart_dma_write(const led_spi_dev *const controller);
static void led_usart_tx_isr(void *);

led_spi_dev *led_usart_init(USART_t *const usart, const int8_t dma_ch) {

	uint16_t bsel;
	led_spi_dev *controller = NULL;

	uart_port_desc port_info = uart_port_info(usart);
	uint8_t sclk_bm = 1 << port_info.xck_pin;
	uint8_t sout_bm = 1 << port_info.tx_pin;
	// yeah, I know ;)...
	volatile uint8_t *port_pinctrl = &port_info.port->PIN0CTRL;

	if (dma_ch >= 0) {
		DMA_CH_t *dma_channels = &DMA.CH0;

		bsel = 1;

		DMA.CTRL |= DMA_ENABLE_bm;

		// DMA source address is incremented during transaction and reset at the
		// end of every transaction; dest address is fixed.
		dma_channels[dma_ch].ADDRCTRL =
			DMA_CH_SRCRELOAD_TRANSACTION_gc | DMA_CH_DESTRELOAD_TRANSACTION_gc |
			DMA_CH_SRCDIR_INC_gc | DMA_CH_DESTDIR_FIXED_gc;

		dma_channels[dma_ch].TRIGSRC = usart_dma_get_trigsrc(usart);
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

		led_usart_driver *dev;
		if ((dev = smalloc(sizeof *dev))) {
			// whoa it's a cirlce!
			controller = &dev->controller;
			controller->dev = dev;
			dev->usart = usart;

			controller->load = led_usart_load;
			controller->write = led_usart_write;

			uart_register_handler(uart_get_index(usart), UART_DRE_VEC, led_usart_tx_isr, dev);
		}
	}

	port_info.port->OUTSET = sout_bm | sclk_bm;
	port_info.port->DIRSET = sout_bm | sclk_bm;
	port_pinctrl[port_info.xck_pin] = PORT_INVEN_bm;

	usart->BAUDCTRLA = (uint8_t)(bsel & 0x00FF);
	usart->BAUDCTRLB = (uint8_t)((bsel >> 8) & 0x000F);
	usart->CTRLC |= USART_CMODE_MSPI_gc;
	usart->CTRLB |= USART_TXEN_bm;

	return controller;
}

static void led_usart_load(const led_spi_dev *const controller, const uint8_t *const data, const uint8_t size) {
	led_usart_driver *dev = controller->dev;
	dev->data = data;
	dev->data_size = size;
}

static void led_usart_write(const led_spi_dev *const controller) {
	led_usart_driver *dev = controller->dev;

	dev->usart->CTRLA &= ~USART_DREINTLVL_LO_gc;
	dev->bytes_written = 0;
	dev->usart->CTRLA |= USART_DREINTLVL_LO_gc;
}

static void led_usart_tx_isr(void *const _dev) {
	led_usart_driver *const dev = _dev;

	dev->usart->DATA = dev->data[dev->bytes_written++];

	if (dev->bytes_written >= dev->data_size)
		dev->usart->CTRLA &= ~USART_DREINTLVL_LO_gc;
}

static void led_usart_dma_load(const led_spi_dev *const controller, const uint8_t *const data, const uint8_t size) {
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

static void led_usart_dma_write(const led_spi_dev *const controller) {
	((DMA_CH_t*)(controller->dev))->CTRLA |= DMA_CH_ENABLE_bm;
}
