#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include <string.h>

#include "util.h"
#include "string.h"
#include "serialcomm.h"
#include "spi.h"
#include "mempool.h"
#include "displaycomm.h"

#include "display.h"


#define DISPLAY_SPI SPIC
#define DISPLAY_SPI_vect SPIC_INT_vect

static serialcomm_t *comm;
static spi_master_t *spi;
static mempool_t *mempool;

void display_init(void) {
	spi = spi_master_init(&DISPLAY_SPI, 8, SPI_PRESCALER_DIV128_gc);
	mempool = init_mempool(DISPLAY_PKT_MAX_SIZE, 4);

	serialcomm_driver driver = {
		.dev = spi,
		.rx_func = NULL,
		.tx_func = (void (*)(void *, uint8_t *, uint8_t, void (*)(void *)))spi_master_write,
	};

	comm = serialcomm_init(driver, 1 /*dummy address*/);
}

void display_send(const uint8_t cmd, const uint8_t size, uint8_t *data) {
	display_pkt *const pkt = mempool_alloc(mempool);

	if (pkt) {
		pkt->cmd = cmd;
		pkt->size = size;

		memcpy(pkt->data, data, pkt->size);

		serialcomm_send(comm, 1, sizeof(*pkt) + pkt->size, (uint8_t*)pkt, mempool_putref);
	}
}

void display_write(char *str) {
	display_send(0, strlen(str) + 1, (uint8_t *)str);
}

ISR(DISPLAY_SPI_vect) {
	spi_master_isr(spi);
}
