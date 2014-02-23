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
#include <leds.h>
#include "irtx.h"
#include <irrx.h>
#include "trigger.h"
#include <tasks.h>
#include <mpc.h>
#include <scheduler.h>
#include <util.h>

static void mpc_reply_ping(const mpc_pkt * const pkt); 

int main(void) {
	sysclk_set_internal_32mhz();

	led_init();
	irtx_init();
	trigger_init();
	irrx_init();
	mpc_init();
	scheduler_init();
	tasks_init();
	mpc_register_cmd('P', mpc_reply_ping);

	PMIC.CTRL |= PMIC_MEDLVLEN_bm | PMIC_LOLVLEN_bm | PMIC_HILVLEN_bm;
	sei();
	
	while(1) {
		tasks_run();	
	/*	if ( trigger_pressed() ) 
			//T = trigger pressed
			phasor_comm_send('T', 0, NULL);

			} else if ( pkt->cmd == 'T' ) {
				uint8_t * foo = (uint8_t*)pkt->data;
				irtx_send((irtx_pkt*)foo);
			}

	*/
	}

	return 0;
}

static void mpc_reply_ping(const mpc_pkt * const pkt) {
	mpc_send_cmd( pkt->saddr, 'R');
}
