#include <stdint.h>

#ifndef GQUEUE_H
#define GQUEUE_H

typedef struct {
	uint8_t size;
	uint8_t read;
	uint8_t write;
	void * items[];
} queue_t;


queue_t * queue_create(const uint8_t size);

uint8_t queue_offer(queue_t * queue, void * data);

void * queue_poll(queue_t * queue);

#define queue_next_idx(idx,size) ((idx == size-1) ? 0 : idx+1)

#endif
