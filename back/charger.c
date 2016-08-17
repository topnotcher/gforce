#include <stdlib.h>

#include <avr/interrupt.h>
#include <avr/io.h>

#include "twi_master.h"
#include <mpc.h>
#include <g4config.h>

#include "charger.h"

#include <FreeRTOS.h>
#include <task.h>

#define CHG_SLAVE_ADDR 0x6B

static void charger_wake_from_isr(void);
static void charger_block(void);
static void charger_task(void *params);

static twi_master_t *twim;

static uint8_t registers[11] = {
	/*@TODO make this nicer*/
	[BQ24193_INPUT_SRC_REG] = 0x30,
	[BQ24193_PWR_ON_REG] = 0x5F,
	[BQ24193_TIME_CTRL_REG] = _BV(7) | _BV(3) | _BV(2) /*0x0A*/,
	[BQ24193_VOLT_CTRL_REG] = _BV(7) | _BV(5) | _BV(4) | _BV(1),

};

static struct {
	uint8_t reg;
	enum {
		OP_WRITE,
		OP_READ,
	} op;
	uint8_t txbuf[2];
} state;

static TaskHandle_t charger_task_handle;

void charger_init(void) {
	//set ~CE low to enable charging
	PORTD.DIRSET = PIN5_bm | PIN6_bm;
	PORTD.OUTCLR = PIN5_bm;
	PORTD.OUTSET = PIN6_bm;

	/* @TODO define this baud rate somewhere*/
	twim = twi_master_init(&TWIE.MASTER, /*155*/ 35, NULL, NULL);
	twi_master_set_blocking(twim, charger_block, charger_wake_from_isr);

	xTaskCreate(charger_task, "charger", 128, NULL, tskIDLE_PRIORITY + 1, &charger_task_handle);
}

static void charger_task(void *params) {
	uint8_t init_regs[4] = {BQ24193_INPUT_SRC_REG, BQ24193_PWR_ON_REG, BQ24193_TIME_CTRL_REG, BQ24193_VOLT_CTRL_REG};

	while (1) {

		for (uint8_t i = 0; i < 4; ++i) {
			charger_write_reg(init_regs[i]);
		}

		uint8_t read_regs[4] = {BQ24193_SYS_STAT_REG, BQ24193_FAULT_REG, BQ24193_FAULT_REG, BQ24193_REVISION_REG};

		for (uint8_t i = 0; i < 4; ++i) {
			charger_read_reg(read_regs[i]);
		}

		mpc_send(MPC_MASTER_ADDR, 'D', 3, &registers[BQ24193_SYS_STAT_REG]);

		// TODO
		charger_block();
	}
}

void charger_read_reg(uint8_t reg) {
	state.reg = reg;
	state.op = OP_READ;
	twi_master_write_read(twim, CHG_SLAVE_ADDR, 1, &state.reg, 1, &registers[reg]);
}

void charger_write_reg(uint8_t reg) {
	state.op = OP_WRITE;
	state.txbuf[0] = reg;
	state.txbuf[1] = registers[reg];

	twi_master_write(twim, CHG_SLAVE_ADDR, 2, state.txbuf);
}

static void charger_wake_from_isr(void) {
	xTaskNotifyFromISR(charger_task_handle, 0, eNoAction, NULL);
}

static void charger_block(void) {
	xTaskNotifyWait(0, 0, NULL, portMAX_DELAY);
}

ISR(TWIE_TWIM_vect) {
	twi_master_isr(twim);
}
