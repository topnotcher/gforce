#include <stdlib.h>

#ifndef MALLOC_H
#define MALLOC_H

typedef struct {
	size_t mem_data;
	size_t mem_heap_stack;
	size_t mem_total;
} mem_usage_t;

void *smalloc(size_t size);
mem_usage_t mem_usage(void);

#endif
