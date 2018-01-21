#include <sam.h>

#ifndef SAML21_DMA_H
#define SAML21_DMA_H

int dma_channel_alloc(void);
DmacDescriptor *dma_channel_desc(int);
void dma_start_transfer(int);

#endif
