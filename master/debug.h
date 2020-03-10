#include <stdint.h>
#include <stdarg.h>

#ifndef SEMIHOST_DEBUG_H
#define SEMIHOST_DEBUG_H

typedef union {
	void *pV;
	const void *cpV;
	char *pC;
	const char *cpC;
	int I;
} SEGGER_SEMIHOST_PARA;

#if !defined(NDEBUG)
#define debug_printf(format, ...) SEGGER_SEMIHOST_Writef(format, __VA_ARGS__)
#define debug_print(msg) SEGGER_SEMIHOST_X_Request(0x04, (uint32_t)(msg))
#else
#define debug_printf(format, ...)
#define debug_print()
#endif

uint32_t __attribute__((noinline)) SEGGER_SEMIHOST_X_Request(uint32_t r0, uint32_t r1);
uint32_t SEGGER_SEMIHOST_Writef(const char *pFormat, ...);
#endif
