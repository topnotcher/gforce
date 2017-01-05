#include <stdlib.h>
#include <string.h>

#include <avr/io.h>
#include <util/crc16.h>
#include <avr/interrupt.h>
#include <twi_master.h>
#include <g4config.h>
#include "config.h"
#include "ds2483.h"
#include "ibutton.h"

#include "xmega/util.h"

#include "game.h"

#include "FreeRTOS.h"
#include "task.h"
#include "timer.h"

static TaskHandle_t ibutton_task_handle;

static ds2483_dev_t *onewiredev;
static void ibutton_wake_from_isr(void);

static void ibutton_run(void *params);
static int8_t ibutton_detect_cycle(uint8_t uuid[8]);

static void ibutton_block(void);

static uint8_t ibutton_uuid[8];

void ibutton_init(void) {
	twi_master_t *twim = twi_master_init(&DS2483_TWI.MASTER, MPC_TWI_BAUD, NULL, NULL);
	twi_master_set_blocking(twim, ibutton_block, ibutton_wake_from_isr);
	onewiredev = ds2483_init(twim, &DS2483_SLPZ_PORT, G4_PIN(DS2483_SLPZ_PIN));

	xTaskCreate(ibutton_run, "ibutton", 128, NULL, tskIDLE_PRIORITY + 1, &ibutton_task_handle);
}

/**
 * This is the main ibutton function that loops
 * continuously polling for an ibutton
 */
void ibutton_run(void *params) {
	uint8_t uuid[8];

	ds2483_rst(onewiredev);

	while (1) {
		int8_t result = ibutton_detect_cycle(uuid);

		if (result > 0) {
			// TODO: technically this is bad ju-ju because the game could be
			// using this memory while we're writing to it. Eh. Fuck it.
			memcpy(ibutton_uuid, uuid, sizeof(ibutton_uuid));
			send_game_event(GAME_EVT_MEMBER_LOGIN, &ibutton_uuid);
		}

		vTaskDelay(configTICK_RATE_HZ/2);
	}
}

static int8_t ibutton_detect_cycle(uint8_t uuid[8]) {
	uint8_t result;

	result = ds2483_1w_rst(onewiredev);

	if (!result)
		return -1; //no device

	ds2483_1w_write(onewiredev, IBUTTON_CMD_READ_ROM);

	uint8_t crc = 0;
	for (uint8_t i = 0; i < 8; ++i) {
		result = ds2483_1w_read_byte(onewiredev);

		if (i < 7) {
			crc = _crc_ibutton_update(crc, result);
			uuid[i] = result;
		}
	}

	if (crc == result)
		return 1;
	else
		return -2; //invalid crc
}

static void ibutton_wake_from_isr(void) {
	xTaskNotifyFromISR(ibutton_task_handle, 0, eNoAction, NULL);
}

static void ibutton_block(void) {
	xTaskNotifyWait(0, 0, NULL, portMAX_DELAY);
}

DS2483_INTERRUPT_HANDLER(ISR(TWIE_TWIM_vect), onewiredev)
