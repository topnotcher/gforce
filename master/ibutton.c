#include <stdlib.h>
#include <avr/io.h>
#include <util/crc16.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <twi_master.h>
#include <g4config.h>
#include <util.h>
#include "config.h"
#include "ds2483.h"
#include "ibutton.h"
#include <tasks.h>
#include "display.h"
#include <timer.h>

#include "FreeRTOS.h"
#include "task.h"

#ifndef IBUTTON_SLEEP_MS
#define IBUTTON_SLEEP_MS 300
#endif

static TaskHandle_t ibutton_task_handle;

static ds2483_dev_t *onewiredev;
static void ibutton_wake(void);
static inline void ibutton_sleep(void);

static void ibutton_run(void *params);
static inline int8_t ibutton_detect_cycle(void);

static void ibutton_block(void);

void ibutton_init(void) {
	twi_master_t *twim = twi_master_init(&DS2483_TWI.MASTER, MPC_TWI_BAUD, NULL, NULL);
	twi_master_set_blocking(twim, ibutton_block, ibutton_wake);
	onewiredev = ds2483_init(twim, &DS2483_SLPZ_PORT, G4_PIN(DS2483_SLPZ_PIN));

	xTaskCreate(ibutton_run, "ibutton", 128, NULL, tskIDLE_PRIORITY + 1, &ibutton_task_handle);
}

/**
 * This is the main ibutton function that loops
 * continuously polling for an ibutton
 */
void ibutton_run(void *params) {
	ds2483_rst(onewiredev);

	while (1) {
		int8_t result = ibutton_detect_cycle();

		if (result > 0)
			display_write("Welcome");

		ibutton_sleep();
	}
}

static inline int8_t ibutton_detect_cycle(void) {
	uint8_t result;

	result = ds2483_1w_rst(onewiredev);

	if (!result)
		return -1; //no device

	ds2483_1w_write(onewiredev, IBUTTON_CMD_READ_ROM);

	uint8_t crc = 0;
	for (uint8_t i = 0; i < 8; ++i) {
		result = ds2483_1w_read_byte(onewiredev);

		if (i < 7)
			crc = _crc_ibutton_update(crc, result);
	}

	if (crc == result)
		return 1;
	else
		return -2; //invalid crc
}

static inline void ibutton_sleep(void) {
	add_timer(ibutton_wake, IBUTTON_SLEEP_MS, 1);
	ibutton_block();
}

/**
 * Wakes the process when I/O is complete
 * or when IBUTTON_SLEEP_MS expires
 */
static void ibutton_wake(void) {
	xTaskNotifyFromISR(ibutton_task_handle, 0, eNoAction, NULL);
}

static void ibutton_block(void) {
	xTaskNotifyWait(0, 0, NULL, portMAX_DELAY);
}


DS2483_INTERRUPT_HANDLER(ISR(TWIE_TWIM_vect), onewiredev)
