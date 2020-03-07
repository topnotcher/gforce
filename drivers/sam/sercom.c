#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <sam.h>

#include <drivers/sam/sercom.h>
#include <drivers/sam/dma.h>
#include <drivers/sam/isr.h>

// TODO: HACK for SAMD51 (just to get it building). The SAML21 uses one IRQ for
// the peripheral. SAMD51 uses 4. It defines 7 interrupt sources. sources 0,1,2
// map to IRQs 0,1,2 while sources 3,4,5,6 map to IRQ 3. The interrupt source
// corresponds to the bit position in the INTFLAG register.
// (DS: 10.2.2: Interrupt Lien Mapping)
#if defined(__SAMD51N20A__)
const int SERCOM0_IRQn = SERCOM0_0_IRQn;
#endif

static sercom_t sercom_inst[SERCOM_INST_NUM];

static void sercom_isr_handler(void);
static void sercom_dummy_handler(sercom_t *sercom);

static Sercom *get_hw_instance(const int idx) {
	if (idx >= 0 && idx < SERCOM_INST_NUM) {
		return (Sercom *const[])SERCOM_INSTS[idx];
	} else {
		return NULL;
	}
}


sercom_t *sercom_init(const int sercom_index) {
	static bool sercom_table_initialized;
	sercom_t *inst = NULL;

	if (!sercom_table_initialized) {
		memset(sercom_inst, 0, sizeof(sercom_inst));

		for (int i = 0; i < SERCOM_INST_NUM; ++i) {
			sercom_inst[i].isr = sercom_dummy_handler;
		}

		sercom_table_initialized = true;
	}

	if (sercom_index >= 0 && sercom_index < SERCOM_INST_NUM) {
		inst = &sercom_inst[sercom_index];

		inst->hw = (Sercom*)get_hw_instance(sercom_index);
		inst->index = sercom_index;

		sercom_enable_pm(inst);
	}

	return inst;
}

int sercom_get_index(const Sercom *const sercom) {
	for (int i = 0; i < SERCOM_INST_NUM; ++i) {
		if (get_hw_instance(i) == sercom)
			return i;
	}

	return -1;
}

void sercom_register_handler(sercom_t *sercom, void (*isr)(sercom_t *)) {
	if (sercom != NULL) {
		sercom->isr = isr;
		nvic_register_isr(SERCOM0_IRQn + sercom->index, sercom_isr_handler);
	}
}

void sercom_enable_irq(const sercom_t *const sercom) {
	NVIC_EnableIRQ(SERCOM0_IRQn + sercom->index);
}

void sercom_disable_irq(const sercom_t *sercom) {
	NVIC_DisableIRQ(SERCOM0_IRQn + sercom->index);
}

void sercom_enable_pm(const sercom_t *const sercom) {

// TODO: this would work better with a series define
#if defined(__SAML21E17B__)
	if (sercom->index >= 0 && sercom->index < 5) {
		MCLK->APBCMASK.reg |= 1 << (sercom->index + MCLK_APBCMASK_SERCOM0_Pos);
	} else if (sercom->index == 5) {
		MCLK->APBDMASK.reg |= MCLK_APBDMASK_SERCOM5;
	}
#elif defined(__SAMD51N20A__)
	if (sercom->index >= 0 && sercom->index < 2) {
		MCLK->APBAMASK.reg |= 1 << (sercom->index + MCLK_APBAMASK_SERCOM0_Pos);
	} else if (sercom->index >= 2 && sercom->index < 4) {
		MCLK->APBBMASK.reg |= 1 << (sercom->index - 2 + MCLK_APBBMASK_SERCOM2_Pos);
	} else if (sercom->index >= 4 && sercom->index <= 7) {
		MCLK->APBDMASK.reg |= 1 << (sercom->index - 4 + MCLK_APBDMASK_SERCOM4_Pos);
	}
#else
	#error "Part not supported!"
#endif
}

void sercom_set_gclk_core(const sercom_t *const sercom, int gclk_gen) {
	int gclk_index = -1;

#if defined(__SAML21E17B__)
	if (sercom->index >= 0 && sercom->index <= SERCOM_INST_NUM) {
		gclk_index = sercom->index + SERCOM0_GCLK_ID_CORE;
	}
#elif defined(__SAMD51N20A__)
	if (sercom->index >= 0 && sercom->index < 2) {
		gclk_index = SERCOM0_GCLK_ID_CORE + sercom->index;
	} else if (sercom->index >= 2 && sercom->index < 4) {
		gclk_index = SERCOM2_GCLK_ID_CORE + sercom->index - 2;
	} else if (sercom->index >= 4 && sercom->index < 7) {
		gclk_index = SERCOM4_GCLK_ID_CORE + sercom->index - 4;
	}
#else
	#error "Part not supported!"
#endif

	if (gclk_index >= 0) {

		// disable peripheral clock channel
		GCLK->PCHCTRL[gclk_index].reg &= ~GCLK_PCHCTRL_CHEN;
		while (GCLK->PCHCTRL[gclk_index].reg & GCLK_PCHCTRL_CHEN);

		// set peripheral clock thannel generator to gclk_gen
		GCLK->PCHCTRL[gclk_index].reg = gclk_gen;

		GCLK->PCHCTRL[gclk_index].reg |= GCLK_PCHCTRL_CHEN;
		while (!(GCLK->PCHCTRL[gclk_index].reg & GCLK_PCHCTRL_CHEN));
	}
}

void sercom_set_gclk_slow(const sercom_t *const sercom, int gclk_gen) {
	static bool sercom_gclk_slow_initialized;

	bool *initialized = NULL;
	int gclk_id = -1;


#if defined(__SAML21E17B__)
	static bool sercom5_gclk_slow_initialized;
	if (sercom->index >= 0 && sercom->index < 5) {
		gclk_id = SERCOM0_GCLK_ID_SLOW;
		initialized = &sercom_gclk_slow_initialized;
	} else if (sercom->index == 5) {
		gclk_id = SERCOM5_GCLK_ID_SLOW;
		initialized = &sercom5_gclk_slow_initialized;
	}
#elif defined(__SAMD51N20A__)
	if (sercom->index >= 0 && sercom->index < SERCOM_INST_NUM) {
		gclk_id = SERCOM0_GCLK_ID_SLOW;
		initialized = &sercom_gclk_slow_initialized;
	}
#else
	#error "Part not supported!"
#endif

	if (initialized && !(*initialized) && gclk_id >= 0) {
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

int sercom_dma_rx_trigsrc(const sercom_t *const sercom) {
#if defined(__SAML21E17B__)
	// sercom5 does not support DMA
	if (sercom->index >= 0 && sercom->index < 5)
		return SERCOM0_DMAC_ID_RX + (2 * sercom->index);
	else
		return -1;
#elif defined(__SAMD51N20A__)
	if (sercom->index >= 0 && sercom->index < SERCOM_INST_NUM)
		return SERCOM0_DMAC_ID_RX + (2 * sercom->index);
	else
		return -1;
#else
	#error "Part not supported!"
#endif
}

int sercom_dma_tx_trigsrc(const sercom_t *const sercom) {
#if defined(__SAML21E17B__)
	// sercom5 does not support DMA
	if (sercom->index >= 0 && sercom->index < 5)
		return SERCOM0_DMAC_ID_TX + (2 * sercom->index);
	else
		return -1;
#elif defined(__SAMD51N20A__)
	if (sercom->index >= 0 && sercom->index < SERCOM_INST_NUM)
		return SERCOM0_DMAC_ID_TX + (2 * sercom->index);
	else
		return -1;
#else
	#error "Part not supported!"
#endif
}

static void sercom_isr_handler(void) {
	const int sercom_index = get_active_irqn() - SERCOM0_IRQn;
	sercom_t *const sercom = &sercom_inst[sercom_index];

	sercom->isr(sercom);
}

static void sercom_dummy_handler(sercom_t *sercom) {
	while (1);
}
