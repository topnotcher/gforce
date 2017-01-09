#include <stdint.h>

#include <avr/io.h>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#ifndef SPI_H
#define SPI_H

struct spi_tx_data {
	const uint8_t *const buf;
	uint8_t size;
	void (*complete)(void *);
};

struct spi_port_desc {
	PORT_t *port;
	uint8_t ss_pin;
	uint8_t mosi_pin;
	uint8_t miso_pin;
	uint8_t sck_pin;
};

typedef struct {
	SPI_t *spi;
	PORT_t *port;
	uint8_t ss_pin;

	uint8_t busy;
	struct spi_tx_data tx;
	uint8_t tx_pos;

	QueueHandle_t tx_queue;
} spi_master_t;

spi_master_t *spi_master_init(SPI_t *, const uint8_t, const uint8_t);
void spi_master_write(spi_master_t *const, const uint8_t *const, const uint8_t, void (*)(void *));
void spi_master_isr(spi_master_t *const);
struct spi_port_desc spi_port_info(const SPI_t *const);

typedef struct {
	QueueHandle_t rx_queue;
	QueueHandle_t tx_queue;

	USART_t *uart;

	// only touchy from interrupt context.
	struct spi_tx_data tx;
	uint8_t tx_pos;
} uart_dev_t;
#endif
