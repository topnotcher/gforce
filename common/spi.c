#include <string.h>

#include "spi.h"
#include "malloc.h"
#include "mempool.h"

#include "FreeRTOS.h"
#include "queue.h"

struct spi_port_desc {
	PORT_t *port;
	uint8_t ss_pin;
	uint8_t mosi_pin;
	uint8_t miso_pin;
	uint8_t sck_pin;
};

static struct spi_port_desc spi_port_info(const SPI_t *const);

spi_master_t *spi_master_init(SPI_t *spi_hw, const uint8_t tx_queue_size, uint8_t prescale) {
	struct spi_port_desc port_info = spi_port_info(spi_hw);
	spi_master_t *dev;
	dev = smalloc(sizeof *dev);

	if (dev) {
		dev->spi = spi_hw;
		dev->port = port_info.port;
		dev->ss_pin = port_info.ss_pin;
		dev->tx_queue = xQueueCreate(tx_queue_size, sizeof(struct spi_tx_data));
		dev->busy = 0;

		// SS will fuck you over hard per xmegaA, pp226
		port_info.port->DIRSET = port_info.mosi_pin | port_info.sck_pin | port_info.ss_pin;
		port_info.port->OUTSET = port_info.mosi_pin | port_info.sck_pin | port_info.ss_pin;

		spi_hw->INTCTRL = SPI_INTLVL_LO_gc;
		spi_hw->CTRL = SPI_ENABLE_bm | SPI_MASTER_bm | prescale;
	}

	return dev;
}

void spi_master_write(spi_master_t *const spi, const uint8_t *const buf, const uint8_t size, void (*complete)(void *)) {
	struct spi_tx_data tx_data = {
		.buf = buf,
		.size = size,
		.complete = complete,
	};

	// busy flag is noodled from the ISR.
	portENTER_CRITICAL();
	if (spi->busy) {
		// spi is busy - queue the data.
		if (!xQueueSend(spi->tx_queue, &tx_data, 0) && complete != NULL)
			complete((void*)buf);
	} else {
		spi->port->OUTCLR = spi->ss_pin;
		spi->busy = 1;
		memcpy(&spi->tx, &tx_data, sizeof(tx_data));
		spi->tx_pos = 1;
		spi->spi->DATA = spi->tx.buf[0];
	}
	portEXIT_CRITICAL();
}

void spi_master_isr(spi_master_t *const spi) {
	if (spi->tx_pos < spi->tx.size) {
		spi->spi->DATA = spi->tx.buf[spi->tx_pos++];

	// Then the last byte in the current buffer was just sent.
	} else {
		spi->port->OUTSET = spi->ss_pin;

		if (spi->tx.complete)
			spi->tx.complete((void*)spi->tx.buf);

		// if there are more buffers queued...
		if (xQueueReceiveFromISR(spi->tx_queue, &spi->tx, NULL)) {
			spi->port->OUTCLR = spi->ss_pin;
			spi->tx_pos = 1;
			spi->spi->DATA = spi->tx.buf[0];
		} else {
			spi->busy = 0;
		}
	}
}

static struct spi_port_desc spi_port_info(const SPI_t *const spi) {
	struct spi_port_desc desc = {
		.port = NULL,
		.ss_pin = 1 << 4,
		.mosi_pin = 1 << 5,
		.miso_pin = 1 << 6,
		.sck_pin = 1 << 7,
	};

#ifdef SPIC
	if (spi == &SPIC) {
		desc.port = &PORTC;
	}
#endif
#ifdef SPID
	if (spi == &SPID) {
		desc.port = &PORTD;
	}
#endif
#ifdef SPIE
	if (spi == &SPIE) {
		desc.port = &PORTE;
	}
#endif
#ifdef SPIF
	if (spi == &SPIF) {
		desc.port = &PORTF;
	}
#endif

	return desc;
}
