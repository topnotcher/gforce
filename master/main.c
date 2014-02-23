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


int main(void) {
	sysclk_set_internal_32mhz();

	scheduler_init();
	sound_init();
	xbee_init();
	mpc_init();
	game_init();
	display_init();
	tasks_init();

	//clear shit by default.
	lights_off();

//	display_write("Good Morning");
//	_delay_ms(500);

	PMIC.CTRL |= PMIC_MEDLVLEN_bm | PMIC_LOLVLEN_bm | PMIC_HILVLEN_bm;
	sei();

	_delay_ms(500);
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
	}
				
//	else if ( pkt->cmd == 'T' ) {
//		uint8_t data[] = {16,3,255, 56, 127 ,138,103,83,0,15,15,68,72,0,44,1,88,113};
//		phasor_comm_send('T', 18,data);
//	}

}

static void xbee_relay_mpc(const mpc_pkt * const pkt) {
	xbee_send(pkt->cmd,pkt->len+sizeof(*pkt),(uint8_t*)pkt);
}
