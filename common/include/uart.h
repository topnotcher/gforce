#include <stdio.h>
#include <avr/io.h>

#include "FreeRTOS.h"
#include "queue.h"

#ifndef UART_H
#define UART_H

struct _uart_tx_vec {
	const uint8_t *const buf;
	uint8_t len;
	void (*complete)(void *buf);
};

typedef struct {
	QueueHandle_t rx_queue;
	QueueHandle_t tx_queue;

	USART_t *uart;

	// only touchy from interrupt context.
	struct _uart_tx_vec tx;
	uint8_t tx_pos;
} uart_dev_t;


void uart_rx_isr(uart_dev_t *const);
void uart_tx_isr(uart_dev_t *const);
uint8_t uart_getchar(const uart_dev_t *const);
void uart_write(const uart_dev_t *const, const uint8_t *const, const uint8_t, void (*)(void *buf));
uart_dev_t *uart_init(USART_t *, const uint8_t, const uint8_t, const uint16_t, const uint8_t);

#endif
