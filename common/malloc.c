#include <malloc.h>

#include <freertos/FreeRTOS.h>

extern uint8_t __heap_start;
extern uint8_t __data_start;
static size_t heap_offset = (size_t)0;

typedef uint8_t __attribute__((aligned)) __aligned;

/**
 * dynamically allocate an unfree-able chunk of memory.  This is intended to be
 * used during boot so any errors should be found when the board turns on.
 */
void *smalloc(size_t size) {
	void *addr;
	size_t rem;

	portENTER_CRITICAL(); {
		addr = &__heap_start + heap_offset;
		heap_offset += size;
		rem = size % __alignof__(__aligned);

		if (rem)
			heap_offset += __alignof__(__aligned) - rem;
	}; portEXIT_CRITICAL();

	//@TODO check for overflow, throw an error
	return addr;
}

mem_usage_t mem_usage(void) {
	mem_usage_t usage;

	// XXX: this depends very much on the linker script placing the sections
	// where we expect.
	usage.mem_data = (size_t)(&__heap_start - &__data_start);
	usage.mem_heap_stack = heap_offset;
	usage.mem_total = 1;
	//usage.mem_total = RAMSIZE;

	return usage;
}
