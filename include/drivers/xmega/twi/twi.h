#include <stdint.h>

#include <avr/io.h>

#ifndef XMEGA_TWI_H
#define XMEGA_TWI_H

/**
 * On devices with two TWI modules, C and E are present, thus are indexes 0 and
 * 1. On devices with 4, TWID and TWIF come next.
 */
#define TWIC_INST_IDX 0 
#define TWIE_INST_IDX 1 
#define TWID_INST_IDX 2 
#define TWIF_INST_IDX 3 

#ifdef TWIF
#define TWI_INST_NUM 4
#else
#define TWI_INST_NUM 2
#endif


int8_t twi_module_index(const TWI_t *);

#endif
