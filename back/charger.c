#include <avr/interrupt.h>
#include <avr/io.h>

#include "twi_master.h"
#include <mpc.h>
#include <g4config.h>

#define CHG_SLAVE_ADDR 0x6B

void charger_init(void);


static void txn_complete(void *, uint8_t);

static twi_master_t * twim;

static uint8_t reg;
static uint8_t result[2];
void charger_init(void) { 
	//set ~CE low to enable charging
	PORTD.DIRSET = PIN5_bm | PIN6_bm;
	PORTD.OUTCLR = PIN5_bm;
	PORTD.OUTSET = PIN6_bm;

	twim = twi_master_init(&TWIE.MASTER, /*155*/ 35, (void*)0, txn_complete);

	//initiate first read of reg 09
//	static uint8_t foo[2] = {0x01, 0x5F};
	static uint8_t foo[5] = {0x01, 0x5F, CHG_SLAVE_ADDR, 0x05, 0x0A};

	twi_master_write(twim, CHG_SLAVE_ADDR, 5, foo);
}

static void txn_complete(void * derp, uint8_t status) {
	static uint8_t i = 0; 

	if ( i == 0 ) {
		reg = 0x09;
		twi_master_write(twim, CHG_SLAVE_ADDR, 1, &reg);
		i = 1;
	//result of first write of register.
	} else if ( i == 1 ) {
		twi_master_read(twim, CHG_SLAVE_ADDR, 1, &result[0]);
		i = 2;
	} else if ( i == 2 ) {
		reg = 0x09;
		twi_master_write(twim, CHG_SLAVE_ADDR, 1, &reg);
		i = 3;
	} else if ( i == 3 ) {
		twi_master_read(twim, CHG_SLAVE_ADDR, 1, &result[1]);
		i = 4;
	} else if ( i == 4 ) {
		mpc_send(MPC_MASTER_ADDR, 'D', 2, result);	
		 i = 5;
//		reg = 0x0A;
		twi_master_write(twim, CHG_SLAVE_ADDR, 1, &reg);
	} else if ( i == 5 ) {
		twi_master_read(twim, CHG_SLAVE_ADDR, 1, &result[0]);
	//	i = 6;
	} else if ( i == 6) {
//		mpc_send(MPC_MASTER_ADDR, 'D', 1, result);
	}
}

ISR(TWIE_TWIM_vect) {
	twi_master_isr(twim);
}
