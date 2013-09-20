#ifndef IR_TX_H
#define IR_TX_H

typedef struct {
	uint8_t size;
	uint8_t repeat;
	uint8_t data[];
} irtx_pkt;

void irtx_init(void);

void irtx_send(irtx_pkt *);
void irtx_put(uint16_t data);


#endif
