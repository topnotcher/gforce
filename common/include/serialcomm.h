#include <stdint.h>
#include <avr/io.h>

#include "mempool.h"

#ifndef SERIALCOMM_H
#define SERIALCOMM_H

typedef struct {
	void *dev;
	uint8_t (*rx_func)(void *);
	void (*tx_func)(void *, uint8_t *, uint8_t, void (*)(void *));
} serialcomm_driver;

typedef struct {
	serialcomm_driver driver;
	mempool_t *tx_hdr_pool;

	void *(*alloc_rx_buf)(uint8_t *);
	void (*rx_callback)(uint8_t, uint8_t *);

	enum {
		SERIAL_RX_SYNC_IDLE,
		SERIAL_RX_SYNC_DADDR,
		SERIAL_RX_SYNC_SIZE,
		SERIAL_RX_SYNC_CHK
	} rx_sync_state;

	enum {
		SERIAL_RX_IDLE,
		SERIAL_RX_RECV,
	} rx_state;

	uint8_t *rx_buf;
	uint8_t rx_buf_size;
	uint8_t rx_buf_offset;

	uint8_t rx_chk;
	uint8_t rx_size;
	uint8_t addr;
} serialcomm_t;

serialcomm_t *serialcomm_init(
	const serialcomm_driver , void *(*)(uint8_t *),
	void (*)(uint8_t, uint8_t *), uint8_t
);
void serialcomm_send(serialcomm_t *, uint8_t, const uint8_t, uint8_t *, void (*)(void *));
#endif
