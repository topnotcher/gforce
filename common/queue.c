#include <util/atomic.h>

#include <malloc.h>
#include "queue.h"

queue_t * queue_create(const uint8_t size) {
	queue_t * q;

	q = (queue_t*)smalloc(sizeof(*q)*size);
	q->size = size;
	q->read = 0;
	q->write = 0;

	for ( uint8_t i = 0; i < size; ++i )
		q->items[i] = NULL;

	return q;
}

uint8_t queue_offer(queue_t * queue, void * data) {

	uint8_t mkay = 0;

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		if ( queue->items[queue->write] == NULL ) {
			mkay = 1;
			queue->items[queue->write] = data; 
			queue->write = queue_next_idx(queue->write, queue->size);
		}
	}

	return mkay;
}

void * queue_poll(queue_t * queue) {
	uint8_t * ret = NULL;

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		if (queue->items[queue->read] != NULL) {
			ret = queue->items[queue->read];
			queue->items[queue->read] = NULL;
			queue->read = queue_next_idx(queue->read, queue->size);
		}
	}

	return ret;
}
