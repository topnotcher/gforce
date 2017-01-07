#include <freertos/FreeRTOS.h>
#include <freertos/portable.h>

#include <malloc.h>

void *pvPortMalloc(size_t size) {
	return smalloc(size);
}

void vPortFree (void *block) {
}
