#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "malloc.h"
#include "uart.h"

typedef struct {
	PORT_t *port;
	uint8_t tx_pin;
	uint8_t rx_pin;
} uart_port_desc;

static void uart_tx_interrupt_disable(const uart_dev_t *const);
static void uart_tx_interrupt_enable(const uart_dev_t *const);
static uart_port_desc uart_port_info(const USART_t *const uart);

uart_dev_t *uart_init(USART_t *uart_hw, const uint8_t tx_queue_size, const uint8_t rx_queue_size,
                     const uint16_t bsel, const uint8_t bscale) {
	uart_dev_t *uart;
	uint8_t ctrla = 0, ctrlb = 0;
	uart_port_desc port_info = uart_port_info(uart_hw);
	uart = smalloc(sizeof *uart);

	uart_hw->CTRLA = 0;

	if (rx_queue_size) {
		uart->rx_queue = xQueueCreate(rx_queue_size, sizeof(uint8_t));
		ctrla |= USART_RXCINTLVL_MED_gc;
		ctrlb |= USART_RXEN_bm;

		// Manual, page 237.
		port_info.port->DIRCLR = port_info.rx_pin;
	}

	if (tx_queue_size) {
		uart->tx_queue = xQueueCreate(tx_queue_size, sizeof(struct _uart_tx_vec));
		ctrlb |= USART_TXEN_bm;

		// Manual, page 237.
		port_info.port->OUTSET = port_info.tx_pin;
		port_info.port->DIRSET = port_info.tx_pin;
	}
	
	uart->uart = uart_hw;
	uart->tx_pos = 0;
	uart->tx.len = 0;

	uart_hw->BAUDCTRLA = (uint8_t)(bsel & 0x00FF);
	uart_hw->BAUDCTRLB = (bscale << USART_BSCALE_gp) | (uint8_t)((bsel >> 8) & 0x0F);

	uart_hw->CTRLC = USART_PMODE_DISABLED_gc | USART_CHSIZE_8BIT_gc;
	uart_hw->CTRLA = ctrla;
	uart_hw->CTRLB = ctrlb;

	return uart;
}

static uart_port_desc uart_port_info(const USART_t *const uart) {
	uart_port_desc desc;

#ifdef USARTC0
	if (uart == &USARTC0) {
		desc.port = &PORTC;
		desc.tx_pin = PIN3_bm;
		desc.rx_pin = PIN2_bm;
	}
#endif
#ifdef USARTC1
	if (uart == &USARTC1) {
		desc.port = &PORTC;
		desc.tx_pin = PIN7_bm;
		desc.rx_pin = PIN6_bm;
	}
#endif
#ifdef USARTD0
	if (uart == &USARTD0) {
		desc.port = &PORTD;
		desc.tx_pin = PIN3_bm;
		desc.rx_pin = PIN2_bm;
	}
#endif
#ifdef USARTD1
	if (uart == &USARTD1) {
		desc.port = &PORTD;
		desc.tx_pin = PIN7_bm;
		desc.rx_pin = PIN6_bm;
	}
#endif
#ifdef USARTE0
	if (uart == &USARTE0) {
		desc.port = &PORTE;
		desc.tx_pin = PIN3_bm;
		desc.rx_pin = PIN2_bm;
	}
#endif
#ifdef USARTE1
	if (uart == &USARTE1) {
		desc.port = &PORTE;
		desc.tx_pin = PIN7_bm;
		desc.rx_pin = PIN6_bm;
	}
#endif
#ifdef USARTF0
	if (uart == &USARTF0) {
		desc.port = &PORTF;
		desc.tx_pin = PIN3_bm;
		desc.rx_pin = PIN2_bm;
	}
#endif
#ifdef USARTF1
	if (uart == &USARTF1) {
		desc.port = &PORTF;
		desc.tx_pin = PIN7_bm;
		desc.rx_pin = PIN6_bm;
	}
#endif

	return desc;
}

void uart_write(const uart_dev_t *const dev, const uint8_t *const data, const uint8_t len, void (*complete)(void *buf)) {
	struct _uart_tx_vec vec = {
		.buf = data,
		.len = len,
		.complete = complete
	};

	if (xQueueSend(dev->tx_queue, &vec, 0))
		uart_tx_interrupt_enable(dev);
	else
		complete((void*)data);
}

uint8_t uart_getchar(const uart_dev_t *const dev) {
	uint8_t data;
	xQueueReceive(dev->rx_queue, &data, portMAX_DELAY);

	return data;
}

static void uart_send_byte(uart_dev_t *const dev) {
	dev->uart->DATA = dev->tx.buf[dev->tx_pos++];

	if (dev->tx_pos == dev->tx.len && dev->tx.complete)
		dev->tx.complete((void*)dev->tx.buf);
}

void uart_tx_isr(uart_dev_t *const dev) {
	if (dev->tx_pos < dev->tx.len) {
		uart_send_byte(dev);

	} else if (xQueueReceiveFromISR(dev->tx_queue, &dev->tx, NULL)) {
		dev->tx_pos = 0;
		uart_send_byte(dev);

	// this actually triggers the DRE interrupt an extra time
	} else {
		uart_tx_interrupt_disable(dev);
	}
}

void uart_rx_isr(uart_dev_t *const dev) {
	uint8_t data = dev->uart->DATA;
	xQueueSendFromISR(dev->rx_queue, &data, NULL);
}

static void uart_tx_interrupt_disable(const uart_dev_t *const dev) {
	dev->uart->CTRLA &= ~USART_DREINTLVL_MED_gc;
}

static void uart_tx_interrupt_enable(const uart_dev_t *const dev) {
	dev->uart->CTRLA |= USART_DREINTLVL_MED_gc;
}
