#include <stdint.h>
#include <util.h>

#ifndef RINGBUF_H
#define RINGBUF_H

#define ringbuf_next_idx(idx,size) ((idx == size-1) ? 0 : idx+1)

//ringbuf_t * buf
#define ringbuf_full(buf) (ringbuf_next_idx((buf)->write, (buf)->size) == (buf)->read)

//ringbuf_t * buf
#define ringbuf_empty(buf) ((buf)->read == (buf)->write)

typedef struct {
	volatile uint8_t read;
	volatile uint8_t write;
	uint8_t size;
	uint8_t data[];
} ringbuf_t;

ringbuf_t * ringbuf_init(const uint8_t size);
void ringbuf_deinit(void);
//void ringbuf_put(ringbuf_t * const buf, uint8_t data);
uint8_t ringbuf_get(ringbuf_t *);

//uint8_t ringbuf_empty(ringbuf_t *);
//uint8_t ringbuf_full(ringbuf_t *);

void ringbuf_flush(ringbuf_t *);

inline ATTR_ALWAYS_INLINE void ringbuf_put(ringbuf_t * const buf, uint8_t data) {
	
	//queue is full. (note: this wastes the current position in the queue.)
	//if full condition is not detected, then overflow will dump the entire queue (meh, just make the queue larger?)
	//if ( ringbuf_full(buf) ) 
	//	return;


	buf->data[buf->write] = data;
	buf->write = ringbuf_next_idx(buf->write,buf->size);
}

#endif
