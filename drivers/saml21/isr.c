#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <saml21/io.h>
#include <drivers/saml21/isr.h>

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
	void *handler_entry = &vector_table.pfnSYSTEM_Handler + irq;
	init_exception_table();

	__DSB();
	memcpy(handler_entry, &handler, sizeof(handler));
	__DSB();
}
