#include <stdio.h>
#include <avr/io.h>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#ifndef UART_H
#define UART_H

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

typedef struct {
	QueueHandle_t rx_queue;
	QueueHandle_t tx_queue;

	USART_t *uart;

	// only touchy from interrupt context.
	struct _uart_tx_vec tx;
	uint8_t tx_pos;
} uart_dev_t;

enum uart_vector {
	UART_DRE_VEC,
	UART_TXC_VEC,
	UART_RXC_VEC,

	UART_NUM_VEC
};


uint8_t uart_getchar(const uart_dev_t *const);
void uart_write(const uart_dev_t *const, const uint8_t *const, const uint8_t, void (*)(void *buf));
uart_dev_t *uart_init(USART_t *, const uint8_t, const uint8_t, const uint16_t, const uint8_t);
uart_port_desc uart_port_info(const USART_t *const uart);

int8_t uart_get_index(const USART_t *) __attribute__((const));
void uart_register_handler(uint8_t, enum uart_vector, void (*)(void *), void *);

#endif
