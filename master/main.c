#include <stdlib.h>
#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
//#include <avr/wdt.h>

#include <g4config.h>
#include <scheduler.h>
#include "lights.h"
#include "sounds.h"
#include "xbee.h"
#include "display.h"
#include "ibutton.h"
#include "krn.h"
#include <phasor_comm.h>
#include <mpc.h>
#include <colors.h>
#include <tasks.h>
#include <util.h>

#include "game.h"

#ifndef DEBUG
#define DEBUG 1
#endif

static inline void process_ib_pkt(mpc_pkt const * const pkt);
static void xbee_relay_mpc(const mpc_pkt * const pkt);

void * volatile main_stack;
void * volatile ibtn_stack;
void * volatile cur_stack;

int main(void) {
	sysclk_set_internal_32mhz();

	scheduler_init();
	sound_init();
	xbee_init();
	mpc_init();
	game_init();
	display_init();
	ibutton_init();
	tasks_init();

	//clear shit by default.
	lights_off();

//	display_write("Good Morning");
//	_delay_ms(500);

	PMIC.CTRL |= PMIC_MEDLVLEN_bm | PMIC_LOLVLEN_bm | PMIC_HILVLEN_bm;
	sei();

//	_delay_ms(500);
	display_write("");

	//wdt_enable(9);
	uint8_t temp = (WDT.CTRL & ~WDT_ENABLE_bm) | WDT_CEN_bm;
	CCP = CCP_IOREG_gc;
	WDT.CTRL = temp;

	//ping hack: master receives a ping reply
	//send it to the xbee. 
	mpc_register_cmd('R', xbee_relay_mpc);

	//relay data for debugging
	mpc_register_cmd('D', xbee_relay_mpc);

	//DERP
	asm volatile(
			"in r28, __SP_L__\n"\
			"in r29, __SP_H__\n"\
			"sts main_stack, r28\n"\
			"sts main_stack+1, r29\n"\
	);

	ibtn_stack = (void*)(((uint8_t*)main_stack)-128); 
	ibtn_stack = task_stack_init(ibtn_stack,ibutton_run);

	while (1) {
		tasks_run();
	//	wdt_reset();
		process_ib_pkt(xbee_recv());
		display_tx();

	}

	return 0;
}


static inline void process_ib_pkt(mpc_pkt const * const pkt) {

	if ( pkt == NULL ) return;
	
	if ( pkt->cmd == 'I' ) {
		//simulate IR by "bouncing" it off of chest
		mpc_send(MPC_CHEST_ADDR,'I', pkt->len, (uint8_t*)pkt->data);
		//process_ir_pkt(pkt);

	//hackish thing.
	//receive a "ping" from the xbee means 
	//send a ping to the board specified by byte 0
	//of the data. That board should then reply...
	} else if ( pkt->cmd == 'P' ) {
		mpc_send_cmd(pkt->data[0], 'P');
	} else if (pkt->cmd == 'O') {
		task_schedule(ibutton_switchto);
	}
}

static void xbee_relay_mpc(const mpc_pkt * const pkt) {
	xbee_send(pkt->cmd,pkt->len+sizeof(*pkt),(uint8_t*)pkt);
}
