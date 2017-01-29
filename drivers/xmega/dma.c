#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

#include <avr/io.h>

#include <drivers/xmega/dma.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define DMA_CHAN_NUM 4

static void dma_init(void);
static inline void dma_handler(uint8_t) __attribute__((always_inline));

static uint8_t dma_channel_alloc_mask;

static struct {
	void (*handler)(void *);
	void *ins;
} dma_vector_table[DMA_CHAN_NUM];

static void dma_init(void) {
	static bool dma_initialized;

	if (!dma_initialized) {
		DMA.CTRL &= ~DMA_ENABLE_bm;

		DMA.CTRL = DMA_RESET_bm;
		DMA.CTRL |= DMA_ENABLE_bm;
	}
}

DMA_CH_t *dma_channel_alloc(void) {
	dma_init();
	DMA_CH_t *chan = NULL;

	vTaskSuspendAll();
	for (uint8_t i = 0; i < DMA_CHAN_NUM; ++i) {
		if (!(dma_channel_alloc_mask & (1 << i))) {
			dma_channel_alloc_mask |= 1 << i;

			chan = &DMA.CH0 + i;

			chan->CTRLA &= ~DMA_CH_ENABLE_bm;
			chan->CTRLA = DMA_CH_RESET_bm;
		}
	}
	xTaskResumeAll();

	return chan;
}

int8_t dma_get_channel_index(const DMA_CH_t *const chan) {

	for (int8_t i = 0; i < DMA_CHAN_NUM; ++i) {
		if (chan == (&DMA.CH0 + i))
			return i;
	}

	return -1;
}

void dma_register_handler(uint8_t dma_chan_index, void (*handler)(void *), void *ins) {
	dma_vector_table[dma_chan_index].handler = handler;
	dma_vector_table[dma_chan_index].ins = ins;
}

static inline void dma_handler(uint8_t idx) {
	dma_vector_table[idx].handler(dma_vector_table[idx].ins);
}

#define CONCAT(a, b, c) a ## b ## c
#define DMA_HANDLER(x) \
void __attribute__((signal)) DMA_CH ## x ## _vect(void) { \
	dma_handler(x); \
}

DMA_HANDLER(0)
DMA_HANDLER(1)
DMA_HANDLER(2)
DMA_HANDLER(3)

//static_assert(DMA_CHAN_NUM == 4, "Expected 4 dma channels!");
