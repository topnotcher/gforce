#include <stdlib.h>
#include <stdint.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
//#include <avr/wdt.h>

#include <g4config.h>
#include <mpc.h>
#include <mpcphasor.h>
#include <mpctwi.h>

#include "sounds.h"
#include "xbee.h"
#include "display.h"
#include "ibutton.h"
#include "game.h"


#include <drivers/xmega/clock.h>
#include <drivers/xmega/twi/master.h>
#include <drivers/xmega/twi/slave.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "config.h"

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

	sound_init();
	mpc_init();
	xbee_init();
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

void mpc_register_drivers(void) {
	const uint8_t twi_tx_mask = MPC_ADDR_RS | MPC_ADDR_LS | MPC_ADDR_CHEST | MPC_ADDR_BACK;

	twi_slave_t *twis = twi_slave_init(&MPC_TWI, MPC_ADDR_BOARD, 0);
	twi_master_t *twim = twi_master_init(&MPC_TWI, MPC_TWI_BAUD);

	mpc_register_driver(mpctwi_init(twim, twis, MPC_ADDR_BOARD, twi_tx_mask));

	mpc_register_driver(mpc_phasor_init());
}

static void xbee_relay_mpc(const mpc_pkt *const pkt) {
	xbee_send_pkt(pkt);
}
