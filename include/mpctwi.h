#include <stdint.h>
#include <avr/io.h>

#include "mempool.h"
#include "queue.h"
#include "comm.h"
#include "twi_master.h"
#include "twi_slave.h"

#include <util.h>


#ifndef TWI_H
#define TWI_H

comm_dev_t * mpctwi_init( TWI_t * dev, const uint8_t addr, const uint8_t mask, const uint8_t baud );

typedef struct {
	TWI_t * twi;
	twi_master_t * twim;
	twi_slave_t * twis;
} mpc_twi_dev;



#endif
