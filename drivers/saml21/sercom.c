#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>

#include <saml21/io.h>
#include <drivers/saml21/sercom.h>

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
