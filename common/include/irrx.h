#ifndef IRRX_H
#define IRRX_H

void irrx_init(void);

typedef struct __attribute__((packed)) {
	uint8_t cmd;
	uint8_t packs;
	uint8_t passkey;
	uint8_t crc;
} ir_hdr_t;

void ir_rx_simulate(const uint8_t size, const uint8_t *const data);

#endif
