#include <malloc.h>
#include <util/atomic.h>

#include "mempool.h"

#define block(pool,i) ( (mempool_block_t*)( ((uint8_t*)pool) + sizeof(*pool) + i*( sizeof(pool->blocks[0]) + pool->block_size) ) )


mempool_t * init_mempool(const uint8_t block_size, const uint8_t size) {
	mempool_t * pool;

	pool = (mempool_t*)smalloc(sizeof(*pool) + size*(sizeof(mempool_block_t)+block_size));
	pool->size = size;
	pool->block_size = block_size;
	
	for (uint8_t i = 0; i < block_size; ++i)
		block(pool,i)->refcnt = 0;

	return pool;
}

void *mempool_alloc(mempool_t * pool) {
	void * ret = NULL;
	
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		for ( uint8_t i = 0; i < pool->size; ++i ) {
			mempool_block_t * block = block(pool,i);
			if ( block->refcnt == 0 ) {
				ret = block->block;
				block->refcnt = 1;
				break;
			}
		}
	}
	
	return ret;
}
