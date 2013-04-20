#include <stdint.h>

#ifndef RINGBUF_H
#define RINGBUF_H

typedef struct {
	volatile uint8_t read;
	volatile uint8_t write;
	uint8_t size;
	uint8_t * data;
} ringbuf_t;

ringbuf_t * ringbuf_init(uint8_t size);
void ringbuf_deinit(ringbuf_t *);
void ringbuf_put(ringbuf_t * const buf, uint8_t data);
uint8_t ringbuf_get(ringbuf_t *);

uint8_t ringbuf_empty(ringbuf_t *);
uint8_t ringbuf_full(ringbuf_t *);

#define ringbuf_next_idx(idx,size) ((idx == size-1) ? 0 : idx+1)
#endif
