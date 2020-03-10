#include <stdarg.h>
#include <stdint.h>
#include <sam.h>
#include "debug.h"

uint32_t __attribute__((noinline)) SEGGER_SEMIHOST_X_Request(uint32_t r0, uint32_t r1) {
	(void)r1;
	asm volatile( "bkpt 0xab" );
	return r0;
}

#ifndef NDEBUG
static void handle_hardfault(uint32_t *const);

void __attribute__((naked)) HardFault_Handler(void) {
	// Either MSP or PSP contains a pointer to the previous stack frame. Which
	// register is indicated by bit 4 of the link register. Figure out which
	// and read the previous frame pointer into r0/reg_stack_top.
	register uint32_t *r0 asm("r0");
	asm volatile("\
		tst     lr, #4 \n\
		ite     eq \n\
		mrseq   r0, msp \n\
		mrsne   r0, psp\n\
		push    {lr}\n\
	" : "=r" (r0));

	handle_hardfault(r0);

	asm volatile("\
		pop   {lr}\n\
		bx    lr\n\
	");
}

static void handle_hardfault(uint32_t *const stack) {
	// Hard Fault was caused by a debug event
	if (SCB->HFSR & SCB_HFSR_DEBUGEVT_Msk) {
		// clear the fault
		SCB->HFSR = SCB_HFSR_DEBUGEVT_Msk;

		// PC = stack_top + 6
		// Modify PC to return to the instruction _after_ BKPT
		*(stack + 6) += 2;
	} else {
		while (1);
	}
}
#endif

// Not supported by my env (J-Link EDU with GDB server)
uint32_t SEGGER_SEMIHOST_Writef(const char *pFormat, ...) {
  SEGGER_SEMIHOST_PARA aPara[2];
  int r;

  va_list args;
  va_start(args, pFormat);

  aPara[0].cpC = pFormat;
  aPara[1].pV  = (void*)&args;
  r = SEGGER_SEMIHOST_X_Request(0x40, (uint32_t)aPara);

  return r;
}
