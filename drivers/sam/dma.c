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
#if defined(__SAML21E17B__)
		DMAC->CTRL.reg &= ~DMAC_CTRL_CRCENABLE;
#endif

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
// TODO HACK - added just to get it building.
#if defined(__SAML21E17B__)
		DMAC->CHID.reg = chan;
		DMAC->CHCTRLA.reg = DMAC_CHCTRLA_SWRST;
		while (DMAC->CHCTRLA.reg & DMAC_CHCTRLA_SWRST);
#elif defined (__SAMD51P20A__)
		DMAC->Channel[chan].CHCTRLA.reg = DMAC_CHCTRLA_SWRST;
		while (DMAC->Channel[chan].CHCTRLA.reg & DMAC_CHCTRLA_SWRST);
#endif
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
// TODO HACK: Just to get it building.
#if defined(__SAML21E17B__)
	DMAC->CHID.reg = dma_chan;
	DMAC->CHCTRLA.reg |= DMAC_CHCTRLA_ENABLE;
#elif defined(__SAMD51P20A__)
	DMAC->Channel[dma_chan].CHCTRLA.reg = DMAC_CHCTRLA_ENABLE;
#endif
	portEXIT_CRITICAL();
}
