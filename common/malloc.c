#include <malloc.h>
#include <util/atomic.h>
#include <avr/io.h>

#include "config.h"

extern uint8_t __heap_start;
extern uint8_t __data_start;
static size_t heap_offset = (size_t)0;

/**
 * dynamically allocate an unfree-able chunk of memory.  This is intended to be
 * used during boot so any errors should be found when the board turns on.
 */
void *smalloc(size_t size) {
	void *addr;

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		addr = &__heap_start + heap_offset;
		heap_offset += size;
	}

	//@TODO check for overflow, throw an error
	return addr;
}

mem_usage_t mem_usage(void) {
	mem_usage_t usage;

	// XXX: this depends very much on the linker script placing the sections
	// where we expect.
	usage.mem_data = (size_t)(&__heap_start - &__data_start) + heap_offset;
	usage.mem_heap_stack = heap_offset;
	usage.mem_total = RAMSIZE;

	return usage;
}
