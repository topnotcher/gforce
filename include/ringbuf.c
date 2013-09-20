#include <stdlib.h>
#include <stdio.h>
#include "ringbuf.h"

/**
 * data and buf are expected to be allocated already so they can be statically allocated.
 */
inline void ringbuf_init(ringbuf_t * buf, uint8_t size, uint8_t * data) {
	buf->size = size;
	buf->read = 0;
	buf->write = 0;
	buf->data = data;

}

inline void ringbuf_flush(ringbuf_t * const buf) {
	buf->read = buf->write = 0;
}

inline void ringbuf_deinit(void) {
	//derp.
}

extern inline void ringbuf_put(ringbuf_t * const buf, uint8_t data);

inline uint8_t ringbuf_get(ringbuf_t * buf) {
	uint8_t data = buf->data[buf->read];

	buf->read = ringbuf_next_idx(buf->read,buf->size);


	return data;
}
