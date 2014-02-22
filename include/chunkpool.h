#include <stdint.h>
#include <stddef.h>
#ifndef CHUNKALLOC_H
#define CHUNKALLOC_H

//internal use 
typedef struct {
	uint8_t refcnt;
	uint8_t chunk[];
} chunkpool_chunk_t;


typedef struct {
	uint8_t size;
	uint8_t buffsize;
	chunkpool_chunk_t chunks[]; 
} chunkpool_t;

chunkpool_t * chunkpool_create(const uint8_t buffsize, const uint8_t chunks);

uint8_t * chunkpool_acquire(chunkpool_t * pool); 

#define chunkpool_decref(buff) do { ((chunkpool_chunk_t *)((uint8_t*)(buff)-offsetof(chunkpool_chunk_t,chunk)))->refcnt--; } while (0)
#define chunkpool_incref(buff) do { ((chunkpool_chunk_t *)((uint8_t*)(buff)-offsetof(chunkpool_chunk_t,chunk)))->refcnt++; } while (0)

#endif
