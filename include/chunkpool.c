#include <malloc.h>
#include <util/atomic.h>

#include "chunkpool.h"

#define chunk(pool,i) ( (chunkpool_chunk_t*)( ((void*)pool) + sizeof(*pool) + i*( sizeof(pool->chunks[0]) + pool->buffsize) ) )


chunkpool_t * chunkpool_create(const uint8_t buffsize, const uint8_t chunks) {
	chunkpool_t * pool;

	pool = (chunkpool_t*)smalloc(sizeof(*pool) + chunks*(sizeof(chunkpool_chunk_t)+buffsize));
	pool->size = chunks;
	pool->buffsize = buffsize;
	
	for (uint8_t i = 0; i < chunks; ++i)
		chunk(pool,i)->refcnt = 0;

	return pool;
}

void *chunkpool_alloc(chunkpool_t * pool) {
	void * ret = NULL;
	
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		for ( uint8_t i = 0; i < pool->size; ++i ) {
			chunkpool_chunk_t * chunk = chunk(pool,i);
			if ( chunk->refcnt == 0 ) {
				ret = chunk->chunk;
				chunk->refcnt = 1;
				break;
			}
		}
	}
	
	return ret;
}
