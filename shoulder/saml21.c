#include <stdint.h>
#include <saml21/io.h>

#include "main.h"

system_init_func(system_software_init) {

	// LED0 on xplained board.
	PORT[0].Group[1].DIRSET.reg = 1 << 10;
	PORT[0].Group[1].OUTCLR.reg = 1 << 10;

	while (1) {
		for (volatile uint64_t i = 0; i < 100000; ++i);

		PORT[0].Group[1].OUTTGL.reg = 1 << 10;
	}
}
