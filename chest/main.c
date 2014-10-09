#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <stdlib.h>
#include <string.h>

#include "g4config.h"
#include "config.h"


/**
 * G4 common includes.
 */
#include <mpc.h>
#include <leds.h>
#include <buzz.h>
#include <irrx.h>
#include <tasks.h>
#include <util.h>
#include <timer.h>

static void mpc_reply_ping(const mpc_pkt * const pkt);
static void mpc_echo_ir(const mpc_pkt * const pkt);

int main(void) {
	sysclk_set_internal_32mhz();

	timer_init();
	mpc_init();
	led_init();
	buzz_init();
	irrx_init(); 
	tasks_init();
	mpc_register_cmd('P', mpc_reply_ping);
	mpc_register_cmd('I', mpc_echo_ir);

	PMIC.CTRL |= PMIC_LOLVLEN_bm | PMIC_MEDLVLEN_bm | /*PMIC.CTRL |*/ PMIC_HILVLEN_bm;
	sei();

	while(1) {
		tasks_run();
	}


	return 0;
}

static void mpc_reply_ping(const mpc_pkt * const pkt) {
	mpc_send_cmd( pkt->saddr, 'R');
}

static void mpc_echo_ir(const mpc_pkt * const pkt) {
	mpc_send(pkt->saddr, 'I', pkt->len, (uint8_t*)pkt->data);
}
