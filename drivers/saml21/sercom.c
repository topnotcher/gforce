#include <stdlib.h>
#include <stdbool.h>

#include <sam.h>

#include <drivers/saml21/sercom.h>
#include <drivers/saml21/dma.h>
#include <drivers/saml21/isr.h>

static struct {
	void *ins;
	void (*isr)(void *ins);
} sercom_isr_table[SERCOM_INST_NUM];

static void sercom_isr_handler(void);
static void sercom_dummy_handler(void *);

int sercom_get_index(const Sercom *const sercom) {
	const Sercom *const instances[] = SERCOM_INSTS;

	for (int i = 0; i < SERCOM_INST_NUM; ++i) {
		if (sercom == instances[i])
			return i;
	}

	return -1;
}

void sercom_register_handler(const int sercom_index, void (*isr)(void *), void *ins) {
	static bool isr_table_initialized;

	if (!isr_table_initialized) {
		for (int i = 0; i < SERCOM_INST_NUM; ++i) {
			sercom_isr_table[i].isr = sercom_dummy_handler;
			sercom_isr_table[i].ins = (void*)i; // for debugging: gets passed to dummy handler.
		}

		isr_table_initialized = true;
	}

	if (sercom_index < SERCOM_INST_NUM) {
		sercom_isr_table[sercom_index].isr = isr;
		sercom_isr_table[sercom_index].ins = ins;
	}

	nvic_register_isr(SERCOM0_IRQn + sercom_index, sercom_isr_handler);
}

void sercom_enable_irq(const int sercom_index) {
	NVIC_EnableIRQ(SERCOM0_IRQn + sercom_index);
}

void sercom_disable_irq(const int sercom_index) {
	NVIC_DisableIRQ(SERCOM0_IRQn + sercom_index);
}

void sercom_enable_pm(int sercom_index) {

// TODO: this would work better with a series define
#if defined(__SAML21E17B__)
	if (sercom_index >= 0 && sercom_index < 5) {
		MCLK->APBCMASK.reg |= 1 << (sercom_index + MCLK_APBCMASK_SERCOM0_Pos);
	} else if (sercom_index == 5) {
		MCLK->APBDMASK.reg |= MCLK_APBDMASK_SERCOM5;
	}
#elif defined(__SAMD51N20A__)
	if (sercom_index >= 0 && sercom_index < 2) {
		MCLK->APBAMASK.reg |= 1 << (sercom_index + MCLK_APBAMASK_SERCOM0_Pos);
	} else if (sercom_index >= 2 && sercom_index < 4) {
		MCLK->APBBMASK.reg |= 1 << (sercom_index + MCLK_APBBMASK_SERCOM2_Pos);
	} else if (sercom_index >= 4 && sercom_index <= 7) {
		MCLK->APBDMASK.reg |= 1 << (sercom_index + MCLK_APBDMASK_SERCOM4_Pos);
	}
#else
 "Part not supported!"
#endif
}

void sercom_set_gclk_core(int sercom_index, int gclk_gen) {
	int gclk_index = sercom_index + SERCOM0_GCLK_ID_CORE;

	// disable peripheral clock channel
	GCLK->PCHCTRL[gclk_index].reg &= ~GCLK_PCHCTRL_CHEN;
	while (GCLK->PCHCTRL[gclk_index].reg & GCLK_PCHCTRL_CHEN);

	// set peripheral clock thannel generator to gclk_gen
	GCLK->PCHCTRL[gclk_index].reg = gclk_gen;

	GCLK->PCHCTRL[gclk_index].reg |= GCLK_PCHCTRL_CHEN;
	while (!(GCLK->PCHCTRL[gclk_index].reg & GCLK_PCHCTRL_CHEN));
}

void sercom_set_gclk_slow(int sercom_index, int gclk_gen) {
	static bool sercom_gclk_slow_initialized;

	bool *initialized = NULL;
	int gclk_id;


#if defined(__SAML21E17B__)
	static bool sercom5_gclk_slow_initialized;
	if (sercom_index >= 0 && sercom_index < 5) {
		gclk_id = SERCOM0_GCLK_ID_SLOW;
		initialized = &sercom_gclk_slow_initialized;
	} else if (sercom_index == 5) {
		gclk_id = SERCOM5_GCLK_ID_SLOW;
		initialized = &sercom5_gclk_slow_initialized;
	}
#elif defined(__SAMD51N20A__)
	if (sercom_index >= 0 && sercom_index < SERCOM_INST_NUM) {
		gclk_id = SERCOM0_GCLK_ID_SLOW;
		initialized = &sercom_gclk_slow_initialized;
	}
#else
#error "Part not supported!"
#endif

	if (initialized && !(*initialized)) {
		// disable peripheral clock channel
		GCLK->PCHCTRL[gclk_id].reg &= ~GCLK_PCHCTRL_CHEN;
		while (GCLK->PCHCTRL[gclk_id].reg & GCLK_PCHCTRL_CHEN);

		// set peripheral clock channel generator to gclk0
		GCLK->PCHCTRL[gclk_id].reg = gclk_gen;

		GCLK->PCHCTRL[gclk_id].reg |= GCLK_PCHCTRL_CHEN;
		while (!(GCLK->PCHCTRL[gclk_id].reg & GCLK_PCHCTRL_CHEN));

		*initialized = true;
	}
}

int sercom_dma_rx_trigsrc(int sercom_index) {
#if defined(__SAML21E17B__)
	// sercom5 does not support DMA
	if (sercom_index >= 0 && sercom_index < 5)
		return SERCOM0_DMAC_ID_RX + (2 * sercom_index);
	else
		return -1;
#elif defined(__SAMD51N20A__)
	if (sercom_index >= 0 && sercom_index < SERCOM_INST_NUM)
		return SERCOM0_DMAC_ID_RX + (2 * sercom_index);
	else
		return -1;
#else
#error "Part not supported!"
#endif
}

int sercom_dma_tx_trigsrc(int sercom_index) {
#if defined(__SAML21E17B__)
	// sercom5 does not support DMA
	if (sercom_index >= 0 && sercom_index < 5)
		return SERCOM0_DMAC_ID_TX + (2 * sercom_index);
	else
		return -1;
#elif defined(__SAMD51N20A__)
	if (sercom_index >= 0 && sercom_index < SERCOM_INST_NUM)
		return SERCOM0_DMAC_ID_TX + (2 * sercom_index);
	else
		return -1;
#else
#error "Part not supported!"
#endif
}

static void sercom_isr_handler(void) {
	const int sercom_index = get_active_irqn() - SERCOM0_IRQn;
	sercom_isr_table[sercom_index].isr(sercom_isr_table[sercom_index].ins);
}

static void sercom_dummy_handler(void *ins) {
	while (1);
}
