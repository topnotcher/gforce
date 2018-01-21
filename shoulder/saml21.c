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
	uint32_t val = 0;

	// Set GCLK_MAIN to run off of ULP 32KHz (enabled on POR)
	val |= GCLK_GENCTRL_SRC_OSCULP32K;
	val |= GCLK_GENCTRL_GENEN;
	val |= GCLK_GENCTRL_DIV(1);

	GCLK->GENCTRL[0].reg = val;

	// Disable OSC16M
	OSCCTRL->OSC16MCTRL.reg &= ~OSCCTRL_OSC16MCTRL_ENABLE;

	// Set OSC16M to 16MHz and enable
	OSCCTRL->OSC16MCTRL.reg = OSCCTRL_OSC16MCTRL_FSEL_16;
	OSCCTRL->OSC16MCTRL.reg |= OSCCTRL_OSC16MCTRL_ENABLE;

	while (!(OSCCTRL->STATUS.reg & OSCCTRL_STATUS_OSC16MRDY));

	// Set GCLK_MAIN to run off of OSC16M @ 16MHz
	val = 0;
	val |= GCLK_GENCTRL_SRC_OSC16M;
	val |= GCLK_GENCTRL_GENEN;
	val |= GCLK_GENCTRL_DIV(1);

	GCLK->GENCTRL[0].reg = val;
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
