#include <stdint.h>
#include <stddef.h>

#include <freertos/FreeRTOS.h>

#ifndef MEMPOOL_H
#define MEMPOOL_H

//internal use 
typedef struct {
	uint8_t refcnt;
	uint8_t block[] __attribute__((aligned));
} mempool_block_t;


typedef struct {
	uint8_t size;
	uint8_t block_size;
	mempool_block_t *blocks; 
} mempool_t;

mempool_t * init_mempool(const uint8_t buffsize, const uint8_t blocks);

void *mempool_alloc(mempool_t * pool); 

/**
 * Get a pointer to the block given membool_block_t.block (buffer)
 */
#define __MEMPOOL_BLOCK(buffer) \
	((mempool_block_t *)((void*)((uint8_t*)(buffer) - offsetof(mempool_block_t, block))))

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
inline void *mempool_getref(void *buffer) {
	portENTER_CRITICAL();
	__MEMPOOL_INCREF(buffer);
	portEXIT_CRITICAL();

	return buffer;
}

/**
 * Remove a reference to a block (i.e. decrement refcnt). This should be called
 * one time to undo mempool_alloc then an additional time ffor each
 * mempool_getref() call. Excess calls to mempool_putref will result in a
 * memory leak.
 */
inline void mempool_putref(void *buffer) {
	portENTER_CRITICAL();
	__MEMPOOL_DECREF(buffer);
	portEXIT_CRITICAL();
}

#endif
