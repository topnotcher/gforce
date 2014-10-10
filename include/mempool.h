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

/**
 * Get a pointer to the block given membool_block_t.block (buffer)
 */
#define __MEMPOOL_BLOCK(buffer) \
	((mempool_block_t *)((uint8_t*)(buffer)-offsetof(mempool_block_t,block)))

#define __MEMPOOL_DECREF(buffer) do { \
	__MEMPOOL_BLOCK(buffer)->refcnt--; \
} while (0)

#define __MEMPOOL_INCREF(buffer) do { \
	__MEMPOOL_BLOCK(buffer)->refcnt++; \
} while (0)

/**
 * Get a "new" reference to a block. This should be called when copying the
 * data.
 */
static inline void * mempool_getref(void *buffer) {
	__MEMPOOL_INCREF(buffer);
	return buffer;
}

/**
 * Remove a reference to a block (i.e. decrement refcnt). This should be called
 * one time to undo mempool_alloc then an additional time ffor each
 * mempool_getref() call. Excess calls to mempool_putref will result in a
 * memory leak.
 */
static inline void mempool_putref(void *buffer) {
	__MEMPOOL_DECREF(buffer);
}

#endif
