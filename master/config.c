#include <stdint.h>
#include <stdlib.h>
#include <sam.h>
#include "config.h"

#include <drivers/sam/port.h>

static const uint32_t pinmux_config[] = {
	// PB05: PACK-ON signal
	PINMUX_PB05A_EIC_EXTINT5,

	//PB06: ~EXT-PWR signal
	PINMUX_PB06A_EIC_EXTINT6,

	// Charger GPIO signals (GP2, GP1, GP0)
	PINMUX_PC13A_EIC_EXTINT13,
	PINMUX_PC14A_EIC_EXTINT14,
	PINMUX_PC15A_EIC_EXTINT15,

	// xbee awake
	PINMUX_PA19A_EIC_EXTINT3,

	// Debug signals (I didn't know these could be muxed?)
	PINMUX_PB30H_CM4_SWO,
	PINMUX_PA30H_CM4_SWCLK,

	// USB
	PINMUX_PA24H_USB_DM,
	PINMUX_PA25H_USB_DP,

	// SD Card
	PINMUX_PA06I_SDHC0_SDCD,
	PINMUX_PA08I_SDHC0_SDCMD,
	PINMUX_PA09I_SDHC0_SDDAT0,
	PINMUX_PA10I_SDHC0_SDDAT1,
	PINMUX_PA11I_SDHC0_SDDAT2,
	PINMUX_PB10I_SDHC0_SDDAT3,
	PINMUX_PB11I_SDHC0_SDCK,

	// I2S
	PINMUX_PA20J_I2S_FS0,
	PINMUX_PA21J_I2S_SDO,
	PINMUX_PB16J_I2S_SCK0,

	// SERCOM 4: LED Controllers (SDO, SCK)
	PINMUX_PB12C_SERCOM4_PAD0,
	PINMUX_PB13C_SERCOM4_PAD1,

	// SERCOM 0: Radio UART (RX, TX, ~RTS, ~CTS)
	PINMUX_PC16D_SERCOM0_PAD1,
	PINMUX_PC17D_SERCOM0_PAD0,
	PINMUX_PC18D_SERCOM0_PAD2,
	PINMUX_PC19D_SERCOM0_PAD3,

	// SERCOM 2: Charger / eeprom I2C
	PINMUX_PA12C_SERCOM2_PAD0,
	PINMUX_PA13C_SERCOM2_PAD1,

	// SERCOM 3: MPC Slave
	PINMUX_PA22C_SERCOM3_PAD0,
	PINMUX_PA23C_SERCOM3_PAD1,

	// SERCOM 1: MPC Master
	PINMUX_PA16C_SERCOM1_PAD0,
	PINMUX_PA17C_SERCOM1_PAD1,

	// SERCOM 7: Phaser UART (DE, RX, TX)
	PINMUX_PB18D_SERCOM7_PAD2,
	PINMUX_PB19D_SERCOM7_PAD3,
	PINMUX_PB21D_SERCOM7_PAD0,

	// Infrared RX UART
	PINMUX_PC05C_SERCOM6_PAD1
};


void configure_pinmux(void) {
	for (size_t i = 0; i < sizeof(pinmux_config)/sizeof(pinmux_config[0]); ++i)
		port_pinmux(pinmux_config[i]);
}

void configure_clocks(void) {
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

	// Set GCLK1 to run off of XOSC1
	val = GCLK_GENCTRL_SRC_XOSC1;
	val |= GCLK_GENCTRL_GENEN;
	val |= GCLK_GENCTRL_DIV(1);

	GCLK->GENCTRL[1].reg = val;

	// wait for the switch to complete
	while (GCLK->SYNCBUSY.reg & GCLK_SYNCBUSY_GENCTRL1);

	// Set GCLK2 to run off off DFLL48M
	val = GCLK_GENCTRL_SRC_DFLL;
	val |= GCLK_GENCTRL_GENEN;
	val |= GCLK_GENCTRL_DIV(1);

	GCLK->GENCTRL[2].reg = val;

	// wait for the switch to complete
	while (GCLK->SYNCBUSY.reg & GCLK_SYNCBUSY_GENCTRL2);





	// Enable the external crystal oscillator XOSC0
	val = OSCCTRL_XOSCCTRL_ENALC;
	val |= OSCCTRL_XOSCCTRL_ENABLE;
	val |= OSCCTRL_XOSCCTRL_IMULT(4);
	val |= OSCCTRL_XOSCCTRL_IPTAT(3);
	val |= OSCCTRL_XOSCCTRL_XTALEN;
	OSCCTRL->XOSCCTRL[0].reg = val;
	while (!(OSCCTRL->STATUS.reg & OSCCTRL_STATUS_XOSCRDY0));

	// Set GCLK3 to run off of XOSC1
	val = GCLK_GENCTRL_SRC_XOSC0;
	val |= GCLK_GENCTRL_GENEN;
	val |= GCLK_GENCTRL_DIV(1);

	GCLK->GENCTRL[3].reg = val;
	while (GCLK->SYNCBUSY.reg & GCLK_SYNCBUSY_GENCTRL3);





	// GCLK[3] from OSCULP32K
	/* val = GCLK_GENCTRL_SRC_OSCULP32K; */
	/* val |= GCLK_GENCTRL_GENEN; */
	/* val |= GCLK_GENCTRL_DIV(1); */
    /*  */
	/* GCLK->GENCTRL[3].reg = val; */

	// wait for the switch to complete
	/* while (GCLK->SYNCBUSY.reg & GCLK_SYNCBUSY_GENCTRL3); */

	/* pm_set_periph_clock(OSCCTRL_GCLK_ID_FDPLL0, GCLK_PCHCTRL_GEN_GCLK3, true); */
	/* pm_set_periph_clock(OSCCTRL_GCLK_ID_FDPLL0, GCLK_PCHCTRL_GEN_GCLK1, true); */

	/* OSCCTRL->Dpll[0].DPLLRATIO.reg = OSCCTRL_DPLLRATIO_LDR(3499); */
	/* OSCCTRL->Dpll[0].DPLLRATIO.reg = OSCCTRL_DPLLRATIO_LDR(9); */
	OSCCTRL->Dpll[0].DPLLRATIO.reg = OSCCTRL_DPLLRATIO_LDR(19);
	while (OSCCTRL->Dpll[0].DPLLSYNCBUSY.reg & OSCCTRL_DPLLSYNCBUSY_DPLLRATIO);

	/* OSCCTRL->Dpll[0].DPLLCTRLB.reg = OSCCTRL_DPLLCTRLB_REFCLK_GCLK | OSCCTRL_DPLLCTRLB_LBYPASS | OSCCTRL_DPLLCTRLB_WUF; */
	OSCCTRL->Dpll[0].DPLLCTRLB.reg = OSCCTRL_DPLLCTRLB_REFCLK_XOSC1 | OSCCTRL_DPLLCTRLB_LBYPASS | OSCCTRL_DPLLCTRLB_WUF;

	OSCCTRL->Dpll[0].DPLLCTRLA.reg = OSCCTRL_DPLLCTRLA_ENABLE;
	while (OSCCTRL->Dpll[0].DPLLSYNCBUSY.reg & OSCCTRL_DPLLSYNCBUSY_ENABLE);

	while (!(OSCCTRL->Dpll[0].DPLLSTATUS.reg & (OSCCTRL_DPLLSTATUS_CLKRDY | OSCCTRL_DPLLSTATUS_LOCK)));

	// GCLK[0] CPU from DPLL0
	val = GCLK_GENCTRL_SRC_DPLL0;
	val |= GCLK_GENCTRL_GENEN;
	val |= GCLK_GENCTRL_DIV(1);

	GCLK->GENCTRL[0].reg = val;

	// wait for the switch to complete
	while (GCLK->SYNCBUSY.reg & GCLK_SYNCBUSY_GENCTRL0);
}
