#include <malloc.h>
#include <util/atomic.h>

#include "chunkpool.h"

chunkpool_t * chunkpool_create(const uint8_t buffsize, const uint8_t chunks) {
	chunkpool_t * pool;

	pool = (chunkpool_t*)smalloc(sizeof(*pool) + chunks*(sizeof(chunkpool_chunk_t)+buffsize));
	pool->size = chunks;
	
	for ( uint8_t i = 0; i < chunks; ++i ) 
		pool->chunks[i].refcnt = 0;

	return pool;
}

uint8_t * chunkpool_acquire(chunkpool_t * pool) {
	uint8_t * ret = NULL;
	
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		for ( uint8_t i = 0; i < pool->size; ++i ) {
			if ( pool->chunks[i].refcnt == 0 ) {
				ret = pool->chunks[i].chunk;
				pool->chunks[i].refcnt = 1;
				break;
			}
		}
	}
	
	return ret;
}
