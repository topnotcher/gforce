#include <stdbool.h>

#include <drivers/sam/dma.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#if defined(__SAML21E17B__)
	static DmacDescriptor __attribute__((section(".lpram"))) dma_descriptors[DMAC_CH_NUM];
	static DmacDescriptor __attribute__((section(".lpram"))) dma_descriptors_write_back[DMAC_CH_NUM];
#elif defined(__SAMD51N20A__)
	static DmacDescriptor dma_descriptors[DMAC_CH_NUM] __attribute__((aligned(128)));
	static DmacDescriptor dma_descriptors_write_back[DMAC_CH_NUM] __attribute__((aligned(128)));
#else
	#error "Unsupported part!"
#endif


static uint32_t dma_channel_alloc_mask;

static void dmac_init(void);

static void dmac_init(void) {
	static bool dmac_initialized;

	if (!dmac_initialized) {
		MCLK->AHBMASK.reg |= MCLK_AHBMASK_DMAC;

		DMAC->CTRL.reg &= ~DMAC_CTRL_DMAENABLE;
#if defined(__SAML21E17B__)
		// TODO: why?
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
	for (uint32_t i = 0; i < DMAC_CH_NUM; ++i) {
		if (!(dma_channel_alloc_mask & (1 << i))) {
			dma_channel_alloc_mask |= 1 << i;

			chan = i;
			break;
		}
	}

	if (chan >= 0) {
// TODO HACK - added just to get it building.
#if defined(__SAML21E17B__)
		// NOTE: assumed this is called from a critical section
		DMAC->CHID.reg = chan;
		DMAC->CHCTRLA.reg = DMAC_CHCTRLA_SWRST;
		while (DMAC->CHCTRLA.reg & DMAC_CHCTRLA_SWRST);
#elif defined (__SAMD51N20A__)
		DMAC->Channel[chan].CHCTRLA.reg = DMAC_CHCTRLA_SWRST;
		while (DMAC->Channel[chan].CHCTRLA.reg & DMAC_CHCTRLA_SWRST);
#else
	#error "Unsupported part!"
#endif
	}
	xTaskResumeAll();

	return chan;
}

DmacDescriptor *dma_channel_desc(int dma_channel) {
	if (dma_channel >= 0 && dma_channel < DMAC_CH_NUM)
		return &dma_descriptors[dma_channel];
	else
		return NULL;
}

void dma_start_transfer(const int dma_chan) {
	dma_descriptors[dma_chan].BTCTRL.reg |= DMAC_BTCTRL_VALID;

// TODO HACK: Just to get it building.
#if defined(__SAML21E17B__)

	portENTER_CRITICAL();
	DMAC->CHID.reg = dma_chan;
	DMAC->CHCTRLA.reg |= DMAC_CHCTRLA_ENABLE;
	portEXIT_CRITICAL();
#elif defined(__SAMD51N20A__)
	DMAC->Channel[dma_chan].CHCTRLA.reg |= DMAC_CHCTRLA_ENABLE;
#else
	#error "Unsupported part"
#endif
}
