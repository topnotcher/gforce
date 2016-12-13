#include <stdlib.h>
#include <avr/io.h>
#include <stdint.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
//#include <avr/wdt.h>

#include <g4config.h>
#include <timer.h>
#include "sounds.h"
#include "xbee.h"
#include "display.h"
#include "ibutton.h"
#include <phasor_comm.h>
#include <mpc.h>
#include <colors.h>
#include <tasks.h>
#include <util.h>

#include "game.h"

#include "FreeRTOS.h"
#include "task.h"

#ifndef DEBUG
#define DEBUG 1
#endif

//static uint8_t rst_reason;

static void xbee_relay_mpc(const mpc_pkt *const pkt);

int main(void) {
	sysclk_set_internal_32mhz();

	PMIC.CTRL |= PMIC_MEDLVLEN_bm | PMIC_LOLVLEN_bm | PMIC_HILVLEN_bm;
	//wdt_enable(9);
	uint8_t temp = (WDT.CTRL & ~WDT_ENABLE_bm) | WDT_CEN_bm;
	CCP = CCP_IOREG_gc;
	WDT.CTRL = temp;

	init_timers();
	sound_init();
	xbee_init();
	mpc_init();
	game_init();

	display_init();

	//ping hack: master receives a ping reply
	//send it to the xbee.
	mpc_register_cmd(MPC_CMD_DIAG_RELAY, xbee_relay_mpc);

	//relay data for debugging
	mpc_register_cmd(MPC_CMD_DIAG_DEBUG, xbee_relay_mpc);

	// memory usage reply...
	mpc_register_cmd(MPC_CMD_DIAG_MEM_USAGE, xbee_relay_mpc);

	ibutton_init();

	vTaskStartScheduler();
}

static void xbee_relay_mpc(const mpc_pkt *const pkt) {
	xbee_send_pkt(pkt);
}
