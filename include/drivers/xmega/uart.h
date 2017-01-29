#include <stdio.h>
#include <avr/io.h>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#ifndef UART_H
#define UART_H

typedef struct _uart_dev_t uart_dev_t;

struct _uart_tx_vec {
	const uint8_t *const buf;
	uint8_t len;
	void (*complete)(void *buf);
};

typedef struct {
	PORT_t *port;
	uint8_t tx_pin;
	uint8_t rx_pin;
	uint8_t xck_pin;
} uart_port_desc;

enum uart_parity_t {
	UART_PARITY_NONE,
	UART_PARITY_EVEN,
	UART_PARITY_ODD,
};

typedef struct {
	// TODO: pass in baud?
	uint16_t bsel;
	int8_t bscale;

	uint8_t bits; // 5- 9
	enum uart_parity_t parity;
	uint8_t stop_bits;

	// 0 to disable rx or tx sides.
	// rx queue size is in bytes.
	uint8_t rx_queue_size;

	// tx queue size is buffers (argh)
	// should I just give in and change that?
	uint8_t tx_queue_size;

	uart_port_desc port_info;
	uint8_t ctrla;
	uint8_t ctrlb;
	uint8_t ctrlc;
} uart_config_t;

enum uart_vector {
	UART_DRE_VEC,
	UART_TXC_VEC,
	UART_RXC_VEC,

	UART_NUM_VEC
};


uint8_t uart_getchar(const uart_dev_t *const);
void uart_write(const uart_dev_t *const, const uint8_t *const, const uint8_t, void (*)(void *buf));
uart_dev_t *uart_init(USART_t *, uart_config_t *) __attribute__((nonnull(1, 2)));
void uart_config_default(uart_config_t *const config) __attribute((nonnull(1)));
uart_port_desc uart_port_info(const USART_t *const uart) __attribute__((nonnull(1)));
uint8_t usart_dma_get_trigsrc(const USART_t *);

int8_t uart_get_index(const USART_t *) __attribute__((const, noinline));
void uart_register_handler(uint8_t, enum uart_vector, void (*)(void *), void *);

#endif
