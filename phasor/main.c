#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <stdlib.h>
#include <string.h>

/**
 * G4 common includes.
 */
#include <g4config.h>
#include <leds.h>
#include <irrx.h>
#include <tasks.h>
#include <mpc.h>
#include <timer.h>
#include <util.h>

#include "irtx.h"
#include "trigger.h"

static void mpc_reply_ping(const mpc_pkt * const pkt); 
static void ir_cmd_tx(const mpc_pkt *  const pkt);

int main(void) {
	sysclk_set_internal_32mhz();

	led_init();
	irtx_init();
	trigger_init();
	irrx_init();
	mpc_init();
	init_timers();
	tasks_init();
	mpc_register_cmd('P', mpc_reply_ping);
	mpc_register_cmd('T', ir_cmd_tx);

	PMIC.CTRL |= PMIC_MEDLVLEN_bm | PMIC_LOLVLEN_bm | PMIC_HILVLEN_bm;
	sei();
	
	while(1) {
		tasks_run();	
		if ( trigger_pressed() ) 
			//T = trigger pressed
			mpc_send_cmd(MPC_MASTER_ADDR, 'T');
	}

	return 0;
}

static void mpc_reply_ping(const mpc_pkt * const pkt) {
	mpc_send_cmd( pkt->saddr, 'R');
}
static void ir_cmd_tx(const mpc_pkt * const pkt) {
	irtx_send((const irtx_pkt*)pkt->data);
}
