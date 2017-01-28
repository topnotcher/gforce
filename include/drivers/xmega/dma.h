#include <avr/io.h>

#ifndef XMEGA_DMA_H
#define XMEGA_DMA_H

DMA_CH_t *dma_channel_alloc(void);

inline __attribute__((always_inline)) void dma_chan_set_destaddr(DMA_CH_t *const chan, const void *const addr) {
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
	chan->DESTADDR0 = ((uint32_t)addr) & 0xFF;
	chan->DESTADDR1 = (((uint32_t)addr) >> 8) & 0xFF;
	chan->DESTADDR2 = (((uint32_t)addr) >> 16) & 0xFF;
	#pragma GCC diagnostic pop
}

inline __attribute__((always_inline)) void dma_chan_set_srcaddr(DMA_CH_t *const chan, const void *const addr) {
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
	chan->SRCADDR0 = ((uint32_t)addr) & 0xFF;
	chan->SRCADDR1 = (((uint32_t)addr) >> 8) & 0xFF;
	chan->SRCADDR2 = (((uint32_t)addr) >> 16) & 0xFF;
	#pragma GCC diagnostic pop
}

inline __attribute__((always_inline)) void dma_chan_set_transfer_count(DMA_CH_t *const chan, uint16_t count) {
	chan->TRFCNTL = count & 0xFF;
	chan->TRFCNTH = (count >> 8) & 0xFF;
}

inline __attribute__((always_inline)) void dma_chan_enable(DMA_CH_t *const chan) {
	chan->CTRLA |= DMA_CH_ENABLE_bm;
}
#endif
