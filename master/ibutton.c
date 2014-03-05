#include <avr/io.h>
#include <util/crc16.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <g4config.h>
#include <util.h>
#include "config.h"
#include "ds2483.h"
#include "ibutton.h"
#include "krn.h"
#include <tasks.h>
#include "display.h"

static ds2483_dev_t * onewiredev;
static void ibutton_txn_complete(ds2483_dev_t * dev, uint8_t status);
static void ibutton_1w_wait(void);

extern void * volatile main_stack; 
extern void * volatile ibtn_stack; 

void ibutton_init(void) {
	onewiredev = ds2483_init(&DS2483_TWI.MASTER, MPC_TWI_BAUD, &DS2483_SLPZ_PORT,G4_PIN(DS2483_SLPZ_PIN), ibutton_txn_complete);
	ds2483_rst(onewiredev);
}

void ibutton_switchto(void) {
	task_context_out();
	main_stack = cur_stack;
	cur_stack = ibtn_stack;
	task_context_in();
	asm volatile ("ret");
}

void ibutton_switchfrom(void) {
	task_context_out();
	ibtn_stack = cur_stack;
	cur_stack = main_stack;
	task_context_in();
	asm volatile ("ret");
}

void ibutton_run(void) {
	while(1) {
		int8_t result = ibutton_detect_cycle();
		if (result > 0)
			display_write("Welcome");
		ibutton_switchfrom();
	}
}

int8_t ibutton_detect_cycle(void) {

	ds2483_1w_rst(onewiredev);
	ibutton_switchfrom();
	//when we wake up here, the command is set, but one wire bus is probably still busy
	ibutton_1w_wait();
	//ibutton_1w_wait should have loaded the status register.
	if (! (onewiredev->result & DS2483_STATUS_PPD) )
		return -1; //no device

	ds2483_1w_write(onewiredev, IBUTTON_CMD_READ_ROM);
	ibutton_switchfrom();
	uint8_t crc = 0;
	//now 8 one wire reads to get ze bytes
	for (uint8_t i = 0; i < 8; ++i) {
		ds2483_1w_read(onewiredev);
		ibutton_switchfrom();
		ibutton_1w_wait();
		ds2483_read_register(onewiredev, DS2483_REGISTER_READ_DATA);
		ibutton_switchfrom();
		//data register read.
		if (i < 7)
			crc = _crc_ibutton_update(crc,onewiredev->result);
	}
	
	if (crc == onewiredev->result)
		return 1;
	else
		return -2; //invalid crc
}

#define IBUTTON_1W_WAIT_TIMEOUT 10
static void ibutton_1w_wait(void) {
	uint8_t tries = 0;
	ds2483_set_read_ptr(onewiredev, DS2483_REGISTER_STATUS);
	ibutton_switchfrom();// swap out while waiting for TWI to complete
	do {
		ds2483_read_byte(onewiredev);
		ibutton_switchfrom();
		asm volatile ( "nop"); 
		asm volatile ( "nop"); 
	} while (tries++ < IBUTTON_1W_WAIT_TIMEOUT && (onewiredev->result & DS2483_STATUS_1WB) );
}


static void ibutton_txn_complete(ds2483_dev_t * dev, uint8_t status) {
	task_schedule(ibutton_switchto);
}

DS2483_INTERRUPT_HANDLER(ISR(TWIE_TWIM_vect), onewiredev)
