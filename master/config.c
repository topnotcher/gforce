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
