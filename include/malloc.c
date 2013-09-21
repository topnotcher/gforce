#include <malloc.h>
#include <util/atomic.h>

#include "config.h"

#ifndef MALLOC_HEAP_SIZE 
	#define MALLOC_HEAP_SIZE 2048 
#endif

static unsigned char heap[MALLOC_HEAP_SIZE];
static size_t heap_offset = (size_t)0;


/**
 * dynamically allocate an unfree-able chunk of memory.
 * This is intended to be used during boot so any errors should be found
 * when the board turns on.
 */
void *smalloc(size_t size) { 
	void * addr;

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		addr = (void*)heap+heap_offset;

		heap_offset += size;
	}

	//@TODO check for overflow, throw an error 	
	return addr;
}
