#include <stdbool.h>

#include <drivers/sam/dma.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define DMAC_CHAN_NUM 16

static DmacDescriptor __attribute__((section(".lpram"))) dma_descriptors[DMAC_CHAN_NUM];
static DmacDescriptor __attribute__((section(".lpram"))) dma_descriptors_write_back[DMAC_CHAN_NUM];

static uint16_t dma_channel_alloc_mask;

static void dmac_init(void);

static void dmac_init(void) {
	static bool dmac_initialized;

	if (!dmac_initialized) {
		MCLK->AHBMASK.reg |= MCLK_AHBMASK_DMAC;

		DMAC->CTRL.reg &= ~DMAC_CTRL_DMAENABLE;
		DMAC->CTRL.reg &= ~DMAC_CTRL_CRCENABLE;

		DMAC->CTRL.reg = DMAC_CTRL_SWRST;

		DMAC->BASEADDR.reg = (uint32_t)&dma_descriptors;
		DMAC->WRBADDR.reg = (uint32_t)&dma_descriptors_write_back;

		DMAC->CTRL.reg = DMAC_CTRL_DMAENABLE | DMAC_CTRL_LVLEN(0xF);
		dmac_initialized = true;
	}
}

int dma_channel_alloc(void) {
	dmac_init();
	int chan = -1;

	vTaskSuspendAll();
	for (uint32_t i = 0; i < DMAC_CHAN_NUM; ++i) {
		if (!(dma_channel_alloc_mask & (1 << i))) {
			dma_channel_alloc_mask |= 1 << i;

			chan = i;
			break;
		}
	}

	if (chan >= 0) {
		DMAC->CHCTRLA.reg &= ~DMAC_CHCTRLA_ENABLE;
		DMAC->CHCTRLA.reg = DMAC_CHCTRLA_SWRST;
	}
	xTaskResumeAll();

	return chan;
}

DmacDescriptor *dma_channel_desc(int dma_channel) {
	if (dma_channel >= 0 && dma_channel < DMAC_CHAN_NUM)
		return &dma_descriptors[dma_channel];
	else
		return NULL;
}

void dma_start_transfer(const int dma_chan) {
	dma_descriptors[dma_chan].BTCTRL.reg |= DMAC_BTCTRL_VALID;
	portENTER_CRITICAL();
	DMAC->CHID.reg = dma_chan;
	DMAC->CHCTRLA.reg |= DMAC_CHCTRLA_ENABLE;
	portEXIT_CRITICAL();
}
