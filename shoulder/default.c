#include "main.h"


void _nopfn(void) {}

/**
 * system_board_init: first function called in main() - sets up clocks etc.
 */
bsp_init_stub(system_board_init);
bsp_init_stub(system_interrupts_enable);
bsp_init_stub(system_configure_os_ticks);
bsp_init_stub(system_software_init);
