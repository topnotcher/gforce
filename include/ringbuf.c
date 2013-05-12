#include <stdlib.h>
#include <stdio.h>
#include "ringbuf.h"

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

extern inline void ringbuf_put(ringbuf_t * const buf, uint8_t data);

inline uint8_t ringbuf_get(ringbuf_t * buf) {
	uint8_t data = buf->data[buf->read];

	buf->read = ringbuf_next_idx(buf->read,buf->size);


	return data;
}
