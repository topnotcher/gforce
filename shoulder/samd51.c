#include <sam.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <drivers/sam/twi/slave.h>
#include <drivers/sam/twi/master.h>
#include <drivers/sam/led_spi.h>

#include <drivers/sam/led_spi.h>

#include <mpc.h>
#include <settings.h>
#include <mpctwi.h>
#include <diag.h>
#include <irrx.h>

#include "main.h"

//static void led_task(void *);

system_init_func(system_board_init) {
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
}

system_init_func(system_software_init) {
	mpc_init();
	settings_init();

	diag_init();
	led_init();
	irrx_init();
}

void mpc_register_drivers(void) {
	uint8_t twi_addr = MPC_ADDR_LS; // TODO

	twi_master_t *twim = twi_master_init(SERCOM1, 14);

	/**
	 * TWI Master PinMux
	 */
	PORT[0].Group[0].PMUX[8].reg = 0x02 | (0x02 << 4);

	PORT[0].Group[0].PINCFG[16].reg |= PORT_PINCFG_PMUXEN;
	PORT[0].Group[0].PINCFG[17].reg |= PORT_PINCFG_PMUXEN;

	twi_slave_t *twis = twi_slave_init(SERCOM3, twi_addr, ~twi_addr);

	/**
	 * TWI Slave PinMux
	 */
	// TODO WRCONFIG
	PORT[0].Group[0].PMUX[11].reg = 0x02 | (0x02 << 4);

	PORT[0].Group[0].PINCFG[22].reg |= PORT_PINCFG_PMUXEN;
	PORT[0].Group[0].PINCFG[23].reg |= PORT_PINCFG_PMUXEN;

	mpc_register_driver(mpctwi_init(twim, twis, twi_addr, MPC_ADDR_MASTER));
}

led_spi_dev *led_init_driver(void) {
	// SERCOM0
	// pa04 - MOSI (SERCOM0:1)
	// pa05 - SCLK (SERCOM0:0)
	PORT[0].Group[0].PMUX[2].reg = 0x03 | (0x03 << 4);

	PORT[0].Group[0].PINCFG[4].reg |= PORT_PINCFG_PMUXEN;
	PORT[0].Group[0].PINCFG[5].reg |= PORT_PINCFG_PMUXEN;

	return led_spi_init(SERCOM0, 0);
}
