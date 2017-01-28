#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include <avr/io.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define DMA_CHAN_NUM 4

static void dma_init(void);

static uint8_t dma_channel_alloc_mask;

static void dma_init(void) {
	static bool dma_initialized;

	if (!dma_initialized) {
		DMA.CTRL |= DMA_ENABLE_bm;
	}
}

DMA_CH_t *dma_channel_alloc(void) {
	dma_init();
	DMA_CH_t *dma_channels = &DMA.CH0;
	DMA_CH_t *chan = NULL;

	vTaskSuspendAll();
	for (uint8_t i = 0; i < DMA_CHAN_NUM; ++i) {
		if (!(dma_channel_alloc_mask & (1 << i))) {
			dma_channel_alloc_mask |= 1 << i;

			chan = dma_channels + i;
		}
	}
	xTaskResumeAll();

	return chan;
}
