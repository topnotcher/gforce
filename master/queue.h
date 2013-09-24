#include <stdint.h>

#ifndef QUEUE_H
#define QUEUE_H

typedef struct {
	uint8_t size;
	uint8_t read;
	uint8_t write;
	uint8_t * items[];
} queue_t;


queue_t * queue_create(const uint8_t size);

uint8_t queue_offer(queue_t * queue, uint8_t * data);

uint8_t * queue_poll(queue_t * queue);

#define queue_next_idx(idx,size) ((idx == size-1) ? 0 : idx+1)

#endif
