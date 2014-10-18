#include <stdlib.h>
#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
//#include <avr/wdt.h>

#include <g4config.h>
#include <timer.h>
#include "lights.h"
#include "sounds.h"
#include "xbee.h"
#include "display.h"
#include "ibutton.h"
#include "threads.h"
#include <phasor_comm.h>
#include <mpc.h>
#include <colors.h>
#include <tasks.h>
#include <util.h>

#include "game.h"

#ifndef DEBUG
#define DEBUG 1
#endif

static void xbee_relay_mpc(const mpc_pkt * const pkt);
static void main_thread(void);

int main(void) {
	sysclk_set_internal_32mhz();

	threads_init();

	init_timers();
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

	main_stack = thread_create(main_thread);
	ibtn_stack = thread_create(ibutton_run);

	cur_stack = main_stack;
	thread_context_in();
	asm volatile ("ret");
}

static void main_thread(void) {
	while (1) {
		tasks_run();
	//	wdt_reset();
		display_tx();
	}
}

static void xbee_relay_mpc(const mpc_pkt * const pkt) {
	xbee_send(pkt->cmd,pkt->len+sizeof(*pkt),(uint8_t*)pkt);
}
