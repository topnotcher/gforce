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

static uint8_t rst_reason;

static void xbee_relay_mpc(const mpc_pkt *const pkt);
static void main_thread(void);
static void why_the_fuck_did_i_reset(void);

int main(void) {
	sysclk_set_internal_32mhz();
	why_the_fuck_did_i_reset();

	init_timers();
	sound_init();
	xbee_init();
	mpc_init();
	game_init();
	ibutton_init();
	display_init();
	tasks_init();

	//clear shit by default.
	lights_off();

	PMIC.CTRL |= PMIC_MEDLVLEN_bm | PMIC_LOLVLEN_bm | PMIC_HILVLEN_bm;
	//interrupts will get enabled when process starts
	//sei();
	//wdt_enable(9);
	uint8_t temp = (WDT.CTRL & ~WDT_ENABLE_bm) | WDT_CEN_bm;
	CCP = CCP_IOREG_gc;
	WDT.CTRL = temp;

	//ping hack: master receives a ping reply
	//send it to the xbee.
	mpc_register_cmd('R', xbee_relay_mpc);

	//relay data for debugging
	mpc_register_cmd('D', xbee_relay_mpc);

	threads_init_stack();
	thread_create("main", main_thread);
	thread_create("ibutton", ibutton_run);
	threads_start_main();
}

static void main_thread(void) {
	display_write("foo");
	while (1) {
		tasks_run();
		//	wdt_reset();
		display_tx();
	}
}

static void xbee_relay_mpc(const mpc_pkt *const pkt) {
	xbee_send(pkt->cmd, pkt->len + sizeof(*pkt), (uint8_t *)pkt);
}

static void why_the_fuck_did_i_reset(void) {
	//spike detection
	if (RST.STATUS & RST_SDRF_bm)
		rst_reason = 1;
	//software reset
	else if (RST.STATUS & RST_SRF_bm)
		rst_reason = 2;
	//PDI reset
	else if (RST.STATUS & RST_PDIRF_bm)
		rst_reason = 3;
	//Watchdog reset
	else if (RST.STATUS & RST_WDRF_bm)
		rst_reason = 4;
	//brown-out reset
	else if (RST.STATUS & RST_BORF_bm)
		rst_reason = 5;
	//external
	else if (RST.STATUS & RST_EXTRF_bm)
		rst_reason = 6;
	//power on reset
	else if (RST.STATUS & RST_PORF_bm)
		rst_reason = 7;
	else
		rst_reason = 0;

	RST.STATUS = 0;
}

ISR(BADISR_vect) {
	while (1);
}
