#include "main.h"


void _nopfn(void) {}

/**
 * system_board_init: first function called in main() - sets up clocks etc.
 */
system_init_stub(system_board_init);
system_init_stub(system_configure_interrupts);
system_init_stub(system_configure_os_ticks);
system_init_stub(system_software_init);
