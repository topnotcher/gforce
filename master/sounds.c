#include <string.h>
#include <stdint.h>
#include <sam.h>

#include <drivers/sam/isr.h>
#include <drivers/sam/port.h>
#include <drivers/sam/dma.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "sounds.h"
#include "config.h"

#include "../../fatfs/ff.h"
#include "sdhc/init.h"
#include "sdhc/sd_mmc.h"

static sound_player player;

static void pm_set_periph_clock(const uint32_t, uint32_t, bool);
static void configure_dma(void);
static void tx_complete(void);
static size_t sound_write_dma_descriptor(const uint8_t);
static size_t sound_fill_outbuf(const uint8_t);
static void mp3_decode_frame(const uint8_t);
static void sound_refill_inbuf(void);
static void sound_task(void *);
static void sound_play_init(void);
static void sound_play_deinit(void);
static void sound_init_i2s(void);
static void sound_init_i2s_frame_params(int, int);


void sound_init(void) {
	memset(&player, 0, sizeof(player));

	sound_init_i2s();

	pin_set_output(PIN_MUTE);
	configure_dma();
	nvic_register_isr(DMAC_0_IRQn + player.dma_chan, tx_complete);
	NVIC_EnableIRQ(DMAC_0_IRQn + player.dma_chan);

	sound_play_deinit();
	xTaskCreate(sound_task, "sound", 8192, NULL, 20, NULL);
}

static void sound_init_i2s_frame_params(int old_samprate, int old_channels) {
	// input clock = 16934400Hz
	static int target_sclks[3][2] = {
		{44100, 23}, // 705600
		{22050, 47}, // 352800
		{48000, 21}, // 705600
	};

	uint32_t txctrl = I2S->TXCTRL.reg;
	uint32_t clkctrl = I2S->CLKCTRL[0].reg;

	if (player.frame_info.samprate != old_samprate) {
		int target = 0;
		for (unsigned int i = 0; i < sizeof(target_sclks)/sizeof(target_sclks[0]); ++i) {
			if (target_sclks[i][0] == player.frame_info.samprate) {
				target = i;
				break;
			}
		}
		clkctrl = (clkctrl & ~I2S_CLKCTRL_MCKDIV_Msk) | I2S_CLKCTRL_MCKDIV(target_sclks[target][1]);
	}

	if (player.frame_info.nChans != old_channels) {
		if (player.frame_info.nChans == 2) {
			txctrl = (txctrl & ~(1u << I2S_TXCTRL_MONO_Pos)) | I2S_TXCTRL_MONO_STEREO;
		} else {
			txctrl = (txctrl & ~(1u << I2S_TXCTRL_MONO_Pos)) | I2S_TXCTRL_MONO_MONO;
		}
	}

	if (txctrl != I2S->TXCTRL.reg || clkctrl != I2S->CLKCTRL[0].reg) {
		I2S->CTRLA.reg &= ~I2S_CTRLA_ENABLE;
		while (I2S->SYNCBUSY.reg & I2S_SYNCBUSY_ENABLE);

		I2S->CLKCTRL[0].reg = clkctrl;
		I2S->TXCTRL.reg = txctrl;

		I2S->CTRLA.reg |= I2S_CTRLA_ENABLE;
		while (I2S->SYNCBUSY.reg & I2S_SYNCBUSY_ENABLE);
	}
}

static void sound_init_i2s(void) {
	uint32_t val;
	MCLK->APBDMASK.reg |= MCLK_APBDMASK_I2S;

	pm_set_periph_clock(I2S_GCLK_ID_0, GCLK_PCHCTRL_GEN_GCLK3, true);

	// SWRST and Enable require this clock!
	pm_set_periph_clock(I2S_GCLK_ID_1, GCLK_PCHCTRL_GEN_GCLK3, true);

	I2S->CTRLA.reg = I2S_CTRLA_SWRST;
	while (I2S->SYNCBUSY.reg & I2S_SYNCBUSY_SWRST);

	val = 0;
	val |= I2S_CLKCTRL_MCKSEL_GCLK;
	val |= I2S_CLKCTRL_SCKSEL_MCKDIV;
	val |= I2S_CLKCTRL_FSSEL_SCKDIV;
	val |= I2S_CLKCTRL_BITDELAY_I2S;
	val |= I2S_CLKCTRL_FSWIDTH_SLOT;
	val |= I2S_CLKCTRL_NBSLOTS(1);
	val |= I2S_CLKCTRL_SLOTSIZE_16;
	I2S->CLKCTRL[0].reg = val;

	val = 0;
	val |= I2S_TXCTRL_BITREV_MSBIT;
	val |= I2S_TXCTRL_DATASIZE_16;
	val |= I2S_TXCTRL_EXTEND_ZERO;
	val |= I2S_TXCTRL_SLOTADJ_RIGHT;
	val |= I2S_TXCTRL_WORDADJ_RIGHT;
	I2S->TXCTRL.reg = val;
}

static void sound_task(void *params) {
	player.task = xTaskGetCurrentTaskHandle();
	static FATFS fs;

	sd_mmc_stack_init();
	while (SD_MMC_OK != sd_mmc_check(0)) {
		/* Wait card ready. */
	}

	f_mount(&fs, "", 0);
	player.decoder = MP3InitDecoder();
	while (!player.decoder) {
		vTaskSuspend(NULL);
	}

	while (1) {
		ulTaskNotifyTake(true, portMAX_DELAY);

		FIL fp;
		FRESULT res = f_open(&fp, player.filename, FA_READ);

		bool more = false;
		if (res == FR_OK) {
			player.fp = &fp;
			sound_play_init();
			more = true;
		}

		while (more) {
			ulTaskNotifyTake(false, portMAX_DELAY);
			player.bufs_full -= 1;

			if (sound_fill_outbuf(player.cur_buf) != 0) {
				player.bufs_full += 1;
				player.cur_buf = (player.cur_buf + 1) % SOUND_NUM_OUT_BUFS;
				sound_refill_inbuf();
			}
			more = player.bufs_full != 0;
		}

		sound_play_deinit();
	}
}

static void sound_play_init(void) {
	pin_set(PIN_MUTE, true);

	player.cur_buf = 0;
	player.bufs_full = 0;
	player.inbuf.size = 0;

	sound_refill_inbuf();

	int old_channels = player.frame_info.nChans;
	int old_samprate = player.frame_info.samprate;

	// circular buffers
	player.dma[SOUND_NUM_OUT_BUFS - 1]->DESCADDR.reg = (uint32_t)player.dma[0];
	for (uint8_t i = 0; i < SOUND_NUM_OUT_BUFS; ++i) {
		if (i > 0) {
			player.dma[i - 1]->DESCADDR.reg = (uint32_t)player.dma[i];
		}

		if (sound_fill_outbuf(i) != 0) {
			player.bufs_full += 1;
			sound_refill_inbuf();

		} else {
			break;
		}
	}

	sound_init_i2s_frame_params(old_samprate, old_channels);

	// Enable transfer complete interrupt, start transfer
	DMAC->Channel[player.dma_chan].CHINTENSET.reg = DMAC_CHINTENSET_TCMPL;
	dma_start_transfer(player.dma_chan);

	// Enable transmitter and clocks
	I2S->CTRLA.reg |= I2S_CTRLA_TXEN | I2S_CTRLA_CKEN0;
}

static void sound_refill_inbuf(void) {
	if (player.fp && player.inbuf.size < SOUND_IN_BUF_LOW_THRESHOLD) {
		// if there is data remaining in the buffer, move it to the beginning.
		if (player.inbuf.size != 0) {
			memmove(&player.inbuf.data[0], player.inbuf.read_ptr, player.inbuf.size);
		}
		player.inbuf.read_ptr = &player.inbuf.data[0];
		uint8_t *write_pos = player.inbuf.read_ptr + player.inbuf.size;
		size_t read_size = SOUND_IN_BUF_SIZE - player.inbuf.size;

		UINT bytes_read = 0;
		FRESULT res = f_read(player.fp, write_pos, read_size, &bytes_read);

		// in the event of an error, just don't update size or anything. We'll
		// assume EOF when the inbuf is < SOUND_IN_BUF_SIZE bytes.
		if (res == FR_OK) {
			player.inbuf.size += bytes_read;	
		} 

		if (bytes_read != read_size) {
			f_close(player.fp);
			player.fp = NULL;
		}
	}
}

static void mp3_decode_frame(const uint8_t buf_num) {
	player.outbufs[buf_num].size = 0;

	if (player.inbuf.size != 0) {
		int err, offset;
		offset = MP3FindSyncWord(player.inbuf.read_ptr, player.inbuf.size);

		if (offset >= 0) {
			player.inbuf.size -= offset;
			player.inbuf.read_ptr += offset;

			err = MP3Decode(
				player.decoder, &player.inbuf.read_ptr,
				&player.inbuf.size, &player.outbufs[buf_num].data[0], 0
			);

			if (err == 0) {
				MP3GetLastFrameInfo(player.decoder, &player.frame_info);
				player.outbufs[buf_num].size = (player.frame_info.outputSamps > SOUND_OUT_BUF_SIZE) 
					? SOUND_OUT_BUF_SIZE : player.frame_info.outputSamps;
			}
			
		} else {
			// TODO: if there's no sync word, assume EOF.
			if (player.fp) {
				f_close(player.fp);
				player.fp = NULL;
			}
			player.inbuf.size = 0;
		}
	}
}

static size_t sound_fill_outbuf(const uint8_t buf_num) {
	mp3_decode_frame(buf_num);
	return sound_write_dma_descriptor(buf_num);
}

static size_t sound_write_dma_descriptor(const uint8_t buf_num) {
	size_t size = player.outbufs[buf_num].size;
	if (size != 0) {
		DmacDescriptor *desc = player.dma[buf_num];

		desc->SRCADDR.reg = (uint32_t)(&player.outbufs[buf_num].data[0] + size);
		desc->BTCNT.reg = size;
		desc->BTCTRL.reg |= DMAC_BTCTRL_VALID;
		player.outbufs[buf_num].size = 0;
	}

	return size;
}

static void sound_play_deinit(void) {
	// Disable channel. TODO: add to dma.c
	DMAC->Channel[player.dma_chan].CHCTRLA.reg &= ~DMAC_CHCTRLA_ENABLE;
	pin_set(PIN_MUTE, false);

	// Disable transfer complete interrupt.
	DMAC->Channel[player.dma_chan].CHINTENCLR.reg = DMAC_CHINTENSET_TCMPL;

	I2S->CTRLA.reg &= ~(I2S_CTRLA_TXEN | I2S_CTRLA_CKEN0);

	if (player.fp) {
		f_close(player.fp);
		player.fp = NULL;
	}
}

void sound_play_effect(char *const effect) {
	player.filename = effect;
	if (player.task)
		xTaskNotify(player.task, 0, eNoAction);
}

void sound_stop(void) {
	sound_play_deinit();
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

static void configure_dma(void) {
	player.dma_chan = dma_channel_alloc();

	if (player.dma_chan >= 0) {

		DmacDescriptor *desc = dma_channel_desc(player.dma_chan);
		desc->BTCTRL.reg =
			DMAC_BTCTRL_BEATSIZE_HWORD |
			DMAC_BTCTRL_SRCINC |
			DMAC_BTCTRL_BLOCKACT_INT;
		desc->BTCNT.reg = 0;
		desc->DSTADDR.reg = (uint32_t)&I2S->TXDATA.reg;

		player.dma[0] = desc;
		for (int i = 0; i < SOUND_NUM_OUT_BUFS - 1; ++i) {
			player.dma[i + 1] = &player.linked_descs[i];
			// copy config to next descriptor
			memcpy(&player.linked_descs[i], desc, sizeof *desc);
		}

		DMAC->Channel[player.dma_chan].CHCTRLA.reg =
			DMAC_CHCTRLA_TRIGSRC(I2S_DMAC_ID_TX_0) |
			DMAC_CHCTRLA_TRIGACT_BURST;
	}
}

static void tx_complete(void) {
	DMAC->Channel[player.dma_chan].CHINTFLAG.reg = DMAC_CHINTFLAG_TCMPL;
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	vTaskNotifyGiveFromISR(player.task, &xHigherPriorityTaskWoken);
	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
