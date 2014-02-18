#include <stdint.h>
#include <avr/io.h>

#include "chunkpool.h"
#include "queue.h"
#include "comm.h"
#include "twi_master.h"

#include <util.h>


#ifndef TWI_H
#define TWI_H

comm_dev_t * twi_init( TWI_t * dev, const uint8_t addr, const uint8_t mask, const uint8_t baud );

comm_frame_t * twi_rx(comm_driver_t * comm);
void twi_slave_isr(comm_driver_t * comm);

typedef struct {
	TWI_t * twi;
	twi_master_t * twim;
} mpc_twi_dev;



#endif
