#include <stdlib.h>
#include <stdint.h>

#include <sam.h>

#include <g4config.h>
#include <mpc.h>
/* #include <mpcphasor.h> */
#include <mpctwi.h>

/* #include "sounds.h" */
/* #include "xbee.h" */
#include "game.h"

#include <drivers/sam/isr.h>

#include <drivers/sam/twi/slave.h>
#include <drivers/sam/twi/master.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "config.h"

#ifndef DEBUG
#define DEBUG 1
#endif

// FreeRTOS hanndlers in port.c
extern void vPortSVCHandler(void) __attribute__(( naked ));
extern void xPortSysTickHandler(void) __attribute__(( naked ));
extern void xPortPendSVHandler(void) __attribute__(( naked ));

/* static void xbee_relay_mpc(const mpc_pkt *const pkt); */
static void configure_clocks(void);

int main(void) {
	configure_clocks();

	// Register the FreeRTOS ISRs.
	nvic_register_isr(SVCall_IRQn, vPortSVCHandler);
	nvic_register_isr(PendSV_IRQn, xPortPendSVHandler);
	nvic_register_isr(SysTick_IRQn, xPortSysTickHandler);

	// Pack on / off
	PORT[0].Group[1].DIRSET.reg = 1 << 4;
	PORT[0].Group[1].OUTSET.reg = 1 << 4;

	/* sound_init(); */
	mpc_init();
	/* xbee_init(); */
	game_init();

	//ping hack: master receives a ping reply
	//send it to the xbee.
	/* mpc_register_cmd(MPC_CMD_DIAG_RELAY, xbee_relay_mpc); */

	//relay data for debugging
	/* mpc_register_cmd(MPC_CMD_DIAG_DEBUG, xbee_relay_mpc); */

	// memory usage reply...
	/* mpc_register_cmd(MPC_CMD_DIAG_MEM_USAGE, xbee_relay_mpc); */

	vTaskStartScheduler();
}

void mpc_register_drivers(void) {
	const uint8_t twi_tx_mask = MPC_ADDR_RS | MPC_ADDR_LS | MPC_ADDR_CHEST | MPC_ADDR_BACK;
	twi_master_t *twim = twi_master_init(SERCOM1, 14);

	/**
	 * TWI Master PinMux
	 */
	PORT[0].Group[0].PMUX[8].reg = 0x02 | (0x02 << 4);

	PORT[0].Group[0].PINCFG[16].reg |= PORT_PINCFG_PMUXEN;
	PORT[0].Group[0].PINCFG[17].reg |= PORT_PINCFG_PMUXEN;

	twi_slave_t *twis = twi_slave_init(SERCOM3, MPC_ADDR_BOARD, twi_tx_mask);

	/**
	 * TWI Slave PinMux
	 */
	// TODO WRCONFIG
	PORT[0].Group[0].PMUX[11].reg = 0x02 | (0x02 << 4);

	PORT[0].Group[0].PINCFG[22].reg |= PORT_PINCFG_PMUXEN;
	PORT[0].Group[0].PINCFG[23].reg |= PORT_PINCFG_PMUXEN;

	mpc_register_driver(mpctwi_init(twim, twis, MPC_ADDR_BOARD, twi_tx_mask));

	/* mpc_register_driver(mpc_phasor_init()); */
}

/* static void xbee_relay_mpc(const mpc_pkt *const pkt) { */
/* 	xbee_send_pkt(pkt); */
/* } */


static void configure_clocks(void) {
	uint32_t val;

	// Ensure DFLL48M is enabled.
	OSCCTRL->DFLLCTRLA.reg = OSCCTRL_DFLLCTRLA_ENABLE;
	while (OSCCTRL->DFLLSYNC.reg & OSCCTRL_DFLLSYNC_ENABLE);

	// Initiate a software reset of the GCLK module.  This will set GCLK0 to
	// DFLL48 @ 48MHz. (enabled above)
	GCLK->CTRLA.reg = GCLK_CTRLA_SWRST;
	while (GCLK->SYNCBUSY.reg & GCLK_SYNCBUSY_SWRST);

	// Enable the external crystal oscillator XOSC1
	val = OSCCTRL_XOSCCTRL_ENALC;
	val |= OSCCTRL_XOSCCTRL_ENABLE;
	val |= OSCCTRL_XOSCCTRL_IMULT(4);
	val |= OSCCTRL_XOSCCTRL_IPTAT(3);
	val |= OSCCTRL_XOSCCTRL_XTALEN;
	OSCCTRL->XOSCCTRL[1].reg = val;
	while (!(OSCCTRL->STATUS.reg & OSCCTRL_STATUS_XOSCRDY1));

	// Set GCLK_MAIN to run off of XOSC1
	val = GCLK_GENCTRL_SRC_XOSC1;
	val |= GCLK_GENCTRL_GENEN;
	val |= GCLK_GENCTRL_DIV(1);

	GCLK->GENCTRL[0].reg = val;

	// wait for the switch to complete
	while (GCLK->SYNCBUSY.reg & GCLK_SYNCBUSY_GENCTRL0);

	// Pack on / off
	PORT[0].Group[1].DIRSET.reg = 1 << 4;
	PORT[0].Group[1].OUTSET.reg = 1 << 4;

	WDT->CTRLA.reg &= ~WDT_CTRLA_ENABLE;
}
