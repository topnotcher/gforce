#include <stdint.h>
#include <stddef.h>
#ifndef MEMPOOL_H
#define MEMPOOL_H

//internal use 
typedef struct {
	uint8_t refcnt;
	uint8_t block[];
} mempool_block_t;


typedef struct {
	uint8_t size;
	uint8_t block_size;
	mempool_block_t blocks[]; 
} mempool_t;

mempool_t * init_mempool(const uint8_t buffsize, const uint8_t blocks);

void *mempool_alloc(mempool_t * pool); 

#define __MEMPOOL_DECREF(block) do { \
	((mempool_block_t *)((uint8_t*)(block)-offsetof(mempool_block_t,block)))->refcnt--; \
} while (0)

#define __MEMPOOL_INCREF(block) do { \
	((mempool_block_t *)((uint8_t*)(block)-offsetof(mempool_block_t,block)))->refcnt++; \
} while (0)

static inline void * mempool_getref(void *block) {
	__MEMPOOL_INCREF(block);
	return block;
}

static inline void mempool_putref(void * block) {
	__MEMPOOL_DECREF(block);
}

#endif
