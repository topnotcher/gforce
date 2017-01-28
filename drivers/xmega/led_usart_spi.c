#include <stdint.h>
#include <stdlib.h>

#include <avr/io.h>

#include <malloc.h>
#include <leds.h>

#include <drivers/xmega/uart.h>
#include <drivers/xmega/led_drivers.h>
#include <drivers/xmega/dma.h>

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

static led_spi_dev *led_usart_dma_init(USART_t *usart, uint16_t *);
static led_spi_dev *led_usart_interrupt_init(USART_t *usart, uint16_t *);

led_spi_dev *led_usart_init(USART_t *const usart, const bool use_dma) {
	uint16_t bsel;
	led_spi_dev *controller = NULL;

	uart_port_desc port_info = uart_port_info(usart);
	uint8_t sclk_bm = 1 << port_info.xck_pin;
	uint8_t sout_bm = 1 << port_info.tx_pin;
	// yeah, I know ;)...
	volatile uint8_t *port_pinctrl = &port_info.port->PIN0CTRL;

	if (use_dma)
		controller = led_usart_dma_init(usart, &bsel);
	else
		controller = led_usart_interrupt_init(usart, &bsel);

	if (controller) {
		port_info.port->OUTSET = sout_bm | sclk_bm;
		port_info.port->DIRSET = sout_bm | sclk_bm;
		port_pinctrl[port_info.xck_pin] = PORT_INVEN_bm;

		usart->BAUDCTRLA = (uint8_t)(bsel & 0x00FF);
		usart->BAUDCTRLB = (uint8_t)((bsel >> 8) & 0x000F);

		usart->CTRLC |= USART_CMODE_MSPI_gc;
		usart->CTRLB |= USART_TXEN_bm;
	}

	return controller;
}

static led_spi_dev *led_usart_dma_init(USART_t *usart, uint16_t *bsel) {
	led_spi_dev *controller = NULL;
	*bsel = 1;

	DMA_CH_t *chan = dma_channel_alloc();

	if (chan) {
		// DMA source address is incremented during transaction and reset at the
		// end of every transaction; dest address is fixed.
		chan->ADDRCTRL =
			DMA_CH_SRCRELOAD_TRANSACTION_gc | DMA_CH_DESTRELOAD_TRANSACTION_gc |
			DMA_CH_SRCDIR_INC_gc | DMA_CH_DESTDIR_FIXED_gc;

		chan->TRIGSRC = usart_dma_get_trigsrc(usart);
		chan->CTRLA = DMA_CH_SINGLE_bm;

		dma_chan_set_destaddr(chan, (void*)&usart->DATA);

		if ((controller = smalloc(sizeof *controller))) {
			controller->write = led_usart_dma_write;
			controller->load = led_usart_dma_load;
			controller->dev = chan;
		}
	}

	return controller;
}

static led_spi_dev *led_usart_interrupt_init(USART_t *usart, uint16_t *bsel) {
	led_spi_dev *controller = NULL;
	led_usart_driver *dev;

	// Without DMA we might prematurely latch the data
	*bsel = 8;

	if ((dev = smalloc(sizeof *dev))) {
		// whoa it's a cirlce!
		controller = &dev->controller;
		controller->dev = dev;
		dev->usart = usart;

		controller->load = led_usart_load;
		controller->write = led_usart_write;

		uart_register_handler(uart_get_index(usart), UART_DRE_VEC, led_usart_tx_isr, dev);
	}

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
	dma_chan_set_srcaddr(controller->dev, data);
	dma_chan_set_transfer_count(controller->dev, size);
}

static void led_usart_dma_write(const led_spi_dev *const controller) {
	dma_chan_enable(controller->dev);
}
