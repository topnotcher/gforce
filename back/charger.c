#include <avr/interrupt.h>
#include <avr/io.h>

#include "twi_master.h"
#include <mpc.h>
#include <g4config.h>

#include "charger.h"

#define CHG_SLAVE_ADDR 0x6B

static void txn_complete(void *, int8_t);

static twi_master_t * twim;

static uint8_t registers[11] = {
	/*@TODO make this nicer*/
	[BQ24193_INPUT_SRC_REG] = 0x30,
	[BQ24193_PWR_ON_REG] = 0x5F,
	[BQ24193_TIME_CTRL_REG] = _BV(7) | _BV(3) | _BV(2)  /*0x0A*/,
	[BQ24193_VOLT_CTRL_REG] = _BV(7)|_BV(5)|_BV(4) | _BV(1),

};

static struct {
	uint8_t reg;
	enum {
		OP_WRITE,
		OP_READ,
	} op;
	volatile uint8_t busy;
	uint8_t txbuf[2];
} state;

void charger_init(void) { 
	//set ~CE low to enable charging
	PORTD.DIRSET = PIN5_bm | PIN6_bm;
	PORTD.OUTCLR = PIN5_bm;
	PORTD.OUTSET = PIN6_bm;
	
	/* @TODO define this baud rate somewhere*/
	twim = twi_master_init(&TWIE.MASTER, /*155*/ 35, (void*)0, txn_complete);

	uint8_t init_regs[4] = {BQ24193_INPUT_SRC_REG, BQ24193_PWR_ON_REG, BQ24193_TIME_CTRL_REG,BQ24193_VOLT_CTRL_REG};

	for (uint8_t i = 0; i < 4; ++i) {
		charger_write_reg(init_regs[i]);
		while (state.busy);
	}
	
	uint8_t read_regs[4] = {BQ24193_SYS_STAT_REG, BQ24193_FAULT_REG, BQ24193_FAULT_REG, BQ24193_REVISION_REG};

	for (uint8_t i = 0; i < 4; ++i) {
		charger_read_reg(read_regs[i]);
		while(state.busy);
	}

	mpc_send(MPC_MASTER_ADDR, 'D', 3, &registers[BQ24193_SYS_STAT_REG]);	
}

void charger_read_reg(uint8_t reg) {
	state.reg = reg;
	state.op = OP_READ;
	state.busy = 1;
	twi_master_write_read(twim, CHG_SLAVE_ADDR, 1, &state.reg, 1, &registers[reg]);
}

void charger_write_reg(uint8_t reg) {
	state.busy = 1;
	state.op = OP_WRITE;
	state.txbuf[0] = reg;
	state.txbuf[1] = registers[reg];

	twi_master_write(twim, CHG_SLAVE_ADDR, 2, state.txbuf);
}

static void txn_complete(void * derp, int8_t status) {
	state.busy = 0;
}

ISR(TWIE_TWIM_vect) {
	twi_master_isr(twim);
}
