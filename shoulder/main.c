
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "main.h"

int main(void) {
	system_board_init();
	system_configure_os_ticks();
	system_configure_interrupts();

	system_software_init();

	vTaskStartScheduler();

	return 0;
}
