#include <stdlib.h>
#include <stdio.h>
#include "ringbuf.h"

#define ringbuf_next_idx(idx,size) ((idx == size-1) ? 0 : idx+1)


inline ringbuf_t * ringbuf_init(uint8_t size) {
	ringbuf_t * buf;

	buf = malloc(sizeof *buf);
	buf->size = size;
	buf->read = 0;
	buf->write = 0;
	buf->data = malloc(size*sizeof(uint8_t));

	return buf;
}
inline void ringbuf_flush(ringbuf_t * const buf) {
	buf->read = buf->write = 0;
}

inline void ringbuf_deinit(ringbuf_t * buf) {
	free(buf->data);
	free(buf);
}

inline void ringbuf_put(ringbuf_t * const buf, uint8_t data) {
	
	//queue is full. (note: this wastes the current position in the queue.)
	if ( ringbuf_full(buf) ) 
		return;

	buf->data[buf->write] = data;
	buf->write = ringbuf_next_idx(buf->write,buf->size);
}

inline uint8_t ringbuf_get(ringbuf_t * buf) {
	uint8_t data = buf->data[buf->read];

	buf->read = ringbuf_next_idx(buf->read,buf->size);


	return data;
}

inline uint8_t ringbuf_empty(ringbuf_t * buf) {
	return buf->read == buf->write;
}

inline uint8_t ringbuf_full(ringbuf_t * buf) {
	return ringbuf_next_idx(buf->write, buf->size) == buf->read;
}
