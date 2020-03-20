#include <sam.h>
#include <drivers/sam/port.h>

#include "../config.h"
#include "../debug.h"
#include "sd_mmc.h"

#include "conf_sd_mmc.h"
#include "hal_mci_sync.h"
#include "init.h"

/* Card Detect (CD) pin settings */
static sd_mmc_detect_t SDMMC_ACCESS_0_cd[CONF_SD_MMC_MEM_CNT] = {

    {PIN_SD_CD, CONF_SD_MMC_0_CD_DETECT_VALUE},
};

/* Write Protect (WP) pin settings */
static sd_mmc_detect_t SDMMC_ACCESS_0_wp[CONF_SD_MMC_MEM_CNT] = {

    {-1, CONF_SD_MMC_0_WP_DETECT_VALUE},
};

static uint8_t sd_mmc_block[512];
static void pm_set_periph_clock(const uint32_t, uint32_t, bool);

static void IO_BUS_PORT_init(void);
static void IO_BUS_init(void);

struct mci_sync_desc IO_BUS;

void IO_BUS_CLOCK_init(void)
{
	MCLK->AHBMASK.reg |= MCLK_AHBMASK_SDHC0;
	pm_set_periph_clock(SDHC0_GCLK_ID, CONF_GCLK_SDHC0_SRC, true);
	pm_set_periph_clock(SDHC0_GCLK_ID_SLOW, CONF_GCLK_SDHC0_SLOW_SRC, true);
}

void IO_BUS_init(void)
{
	IO_BUS_CLOCK_init();
	mci_sync_init(&IO_BUS, SDHC0);
	IO_BUS_PORT_init();
}

void IO_BUS_PORT_init(void)
{

	// TODO: ASF uses this as GPIO, but it can also be MUXed to the module???
	pin_set_input(PIN_SD_CD);
	pin_set(PIN_SD_CD, true);
	pin_set_config(PIN_SD_CD, PORT_PINCFG_INEN | PORT_PINCFG_PULLEN);

	pin_set_output(PIN_SD_POWER_OFF);
	pin_set(PIN_SD_POWER_OFF, false);

}

void SDMMC_ACCESS_0_example(void *_params)
{
	sd_mmc_stack_init();
	while (SD_MMC_OK != sd_mmc_check(0)) {
		/* Wait card ready. */
	}
	if (sd_mmc_get_type(0) & (CARD_TYPE_SD | CARD_TYPE_MMC)) {
		/* Read card block 0 */
		sd_mmc_init_read_blocks(0, 0, 1);
		sd_mmc_start_read_blocks(sd_mmc_block, 1);
		sd_mmc_wait_end_of_read_blocks(false);

		if (sd_mmc_block[510] == 0x55 && sd_mmc_block[511] == 0xAA)
			debug_print("It works!");
	}

#if (CONF_SDIO_SUPPORT == 1)
	if (sd_mmc_get_type(0) & CARD_TYPE_SDIO) {
		/* Read 22 bytes from SDIO Function 0 (CIA) */
		sdio_read_extended(0, 0, 0, 1, sd_mmc_block, 22);
	}
#endif
	while(1) {
		vTaskSuspend(NULL);
	}
}

void sd_mmc_stack_init(void)
{
	IO_BUS_init();
	sd_mmc_init(&IO_BUS, SDMMC_ACCESS_0_cd, SDMMC_ACCESS_0_wp);
}

void pm_set_periph_clock(const uint32_t ph_clock_id, uint32_t src_clock_id, bool enable) {
	// Disable the peripheral clock channel
	GCLK->PCHCTRL[ph_clock_id].reg &= ~GCLK_PCHCTRL_CHEN;
	while (GCLK->PCHCTRL[ph_clock_id].reg & GCLK_PCHCTRL_CHEN);

	// set peripheral clock channel generator to src_clock_id
	GCLK->PCHCTRL[ph_clock_id].reg = src_clock_id;

	if (enable) {
		GCLK->PCHCTRL[ph_clock_id].reg |= GCLK_PCHCTRL_CHEN;
		while (!(GCLK->PCHCTRL[ph_clock_id].reg & GCLK_PCHCTRL_CHEN));
	}
}
