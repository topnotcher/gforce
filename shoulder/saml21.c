#include <stdint.h>
#include <saml21/io.h>
#include <mpc.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <drivers/saml21/twi/slave.h>

#include "main.h"

//static void led_task(void *);

static uint8_t recv_buf[64];

static void begin_txn(void *, uint8_t, uint8_t**, uint8_t*);
static void end_txn(void *, uint8_t, uint8_t*, uint8_t);

static uint8_t count;

system_init_func(system_board_init) {
	uint32_t val = 0;

	// Set GCLK_MAIN to run off of ULP 32KHz (enabled on POR)
	val |= GCLK_GENCTRL_SRC_OSCULP32K;
	val |= GCLK_GENCTRL_GENEN;
	val |= GCLK_GENCTRL_DIV(1);

	GCLK->GENCTRL[0].reg = val;

	// Disable OSC16M
	OSCCTRL->OSC16MCTRL.reg &= ~OSCCTRL_OSC16MCTRL_ENABLE;

	// Set OSC16M to 16MHz and enable
	OSCCTRL->OSC16MCTRL.reg = OSCCTRL_OSC16MCTRL_FSEL_16;
	OSCCTRL->OSC16MCTRL.reg |= OSCCTRL_OSC16MCTRL_ENABLE;

	while (!(OSCCTRL->STATUS.reg & OSCCTRL_STATUS_OSC16MRDY));

	// Set GCLK_MAIN to run off of OSC16M @ 16MHz
	val = 0;
	val |= GCLK_GENCTRL_SRC_OSC16M;
	val |= GCLK_GENCTRL_GENEN;
	val |= GCLK_GENCTRL_DIV(1);

	GCLK->GENCTRL[0].reg = val;
}

system_init_func(system_software_init) {
	twi_slave_t *slave = twi_slave_init();

	slave->begin_txn = begin_txn;
	slave->end_txn = end_txn;

	// LED0 on xplained board.
	/*PORT[0].Group[1].DIRSET.reg = 1 << 10;
	PORT[0].Group[1].OUTCLR.reg = 1 << 10;*/

	//xTaskCreate(led_task, "led task", 256, NULL, tskIDLE_PRIORITY + 1, NULL);
}

static void begin_txn(void *ins, uint8_t write, uint8_t **buf, uint8_t *buf_size) {
	if (!write && !*buf) {
		*buf = recv_buf;
		*buf_size = sizeof(recv_buf);
	}
}

static void end_txn(void *ins, uint8_t write, uint8_t *buf, uint8_t bytes) {
	if (!write && buf) {
		++count;
	}
}
/*
static void led_task(void *params) {
	while (1) {
		PORT[0].Group[1].OUTTGL.reg = 1 << 10;
		vTaskDelay(configTICK_RATE_HZ / 2);
	}
}
*/

/*
void mpc_register_drivers(void) {
	mpc_register_driver(mpctwi_init());
}
*/

