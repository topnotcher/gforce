#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <sam.h>
#include <drivers/sam/isr.h>

static DeviceVectors vector_table __attribute__((aligned(256)));
extern DeviceVectors exception_table;

static void init_exception_table(void) __attribute__((constructor));

static void init_exception_table(void) {
	static bool isr_table_installed;

	if (!isr_table_installed) {
		// Copy the exception table to ram
		memcpy(&vector_table, &exception_table,  sizeof(exception_table));

		// Update the vector table offset to point to the vector table in RAM
		__DSB();
		SCB->VTOR = ((uint32_t)&vector_table) & SCB_VTOR_TBLOFF_Msk;
		__DSB();

		isr_table_installed = true;
	}
}

void nvic_register_isr(const uint32_t irq, void (*const handler)(void)) {
// TODO HACK: just to get it buildling. Might be right though.
#if defined(__SAML21E17B__)
	void *handler_entry = &vector_table.pfnSYSTEM_Handler + irq;
#elif defined(__SAMD51P20A__)
	void *handler_entry = &vector_table.pfnPM_Handler + irq;
#endif
	init_exception_table();

	__DSB();
	memcpy(handler_entry, &handler, sizeof(handler));
	__DSB();
}
