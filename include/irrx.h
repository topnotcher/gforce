#ifndef IRRX_H
#define IRRX_H

void irrx_init(void);

typedef struct {
	uint8_t cmd;
	uint8_t packs;
	uint8_t passkey;
	uint8_t crc;
} ir_hdr_t;


typedef struct {
	uint8_t size;
	uint8_t * data;
} ir_pkt_t;


void ir_rx(ir_pkt_t * pkt);

#endif
