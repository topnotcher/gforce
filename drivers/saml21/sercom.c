#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>

#include <saml21/io.h>
#include <drivers/saml21/sercom.h>
#include <drivers/saml21/dma.h>

#define SERCOM_INST_NUM 6

static struct {
	void *ins;
	void (*isr)(void *ins);
} sercom_isr_table[SERCOM_INST_NUM];

static bool isr_table_initialized;

static inline void sercom_isr_handler(int) __attribute__((always_inline));
static void sercom_dummy_handler(void *);

int sercom_get_index(const Sercom *const sercom) {
	if (sercom == SERCOM0)
		return 0;
	else if (sercom == SERCOM1)
		return 1;
	else if (sercom == SERCOM2)
		return 2;
	else if (sercom == SERCOM3)
		return 3;
	else if (sercom == SERCOM4)
		return 4;
	else if (sercom == SERCOM5)
		return 5;
	else
		return -1;

}

void sercom_register_handler(const int sercom_index, void (*isr)(void *), void *ins) {
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
}

void sercom_enable_irq(const int sercom_index) {
	NVIC_EnableIRQ(SERCOM0_IRQn + sercom_index);
}

void sercom_disable_irq(const int sercom_index) {
	NVIC_DisableIRQ(SERCOM0_IRQn + sercom_index);
}

void sercom_enable_pm(int sercom_index) {
	if (sercom_index != 5) {
		MCLK->APBCMASK.reg |= 1 << (sercom_index + MCLK_APBCMASK_SERCOM0_Pos);
	} else {
		MCLK->APBDMASK.reg |= MCLK_APBDMASK_SERCOM5;
	}
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
	static bool sercom5_gclk_slow_initialized;
	static bool sercom_gclk_slow_initialized;

	bool *initialized;
	int gclk_id;

	if (sercom_index != 5) {
		gclk_id = SERCOM0_GCLK_ID_SLOW;
		initialized = &sercom_gclk_slow_initialized;
	} else {
		gclk_id = SERCOM5_GCLK_ID_SLOW;
		initialized = &sercom5_gclk_slow_initialized;
	}

	if (!(*initialized)) {
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
	if (sercom_index >= 0 && sercom_index < 5)
		return SERCOM0_DMAC_ID_RX + (2 * sercom_index);
	else
		return -1;
}

int sercom_dma_tx_trigsrc(int sercom_index) {
	if (sercom_index >= 0 && sercom_index < 5)
		return SERCOM0_DMAC_ID_TX + (2 * sercom_index);
	else
		return -1;
}

static inline void sercom_isr_handler(const int sercom_index) {
	sercom_isr_table[sercom_index].isr(sercom_isr_table[sercom_index].ins);
}

static void sercom_dummy_handler(void *ins) {
	while (1);
}

#define SERCOM_HANDLER(n) void SERCOM ## n ## _Handler(void) { sercom_isr_handler(n); }

SERCOM_HANDLER(0)
SERCOM_HANDLER(1)
SERCOM_HANDLER(2)
SERCOM_HANDLER(3)
SERCOM_HANDLER(4)
SERCOM_HANDLER(5)
static_assert(SERCOM_INST_NUM == 6, "Expected exactly 6 SERCOMs!!!");
