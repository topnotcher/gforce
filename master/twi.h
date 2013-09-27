#include <stdint.h>
#include <avr/io.h>

#include "chunkpool.h"
#include "queue.h"
#include "comm.h"

#include <util.h>


#ifndef TWI_H
#define TWI_H

typedef struct {
	
	TWI_t * dev;
	chunkpool_t * pool;
	uint8_t mtu;
	uint8_t addr;

	struct {
		/**
		 * Status of the TWI device
		 */
		enum {
			//ideally set by ISR ... Hmmm Just poll the busstate instead?
			//HMM would it be possible to block on the busstate?
			//derpderpderp 
			//HMM i can haz configure buss state timeout?
			//
			TWI_TX_STATE_IDLE = 0,
			TWI_TX_STATE_BUSY = 1
		} state;

		//this needs to be initeddd
		queue_t * queue; 

		comm_frame_t * frame;

		uint8_t byte;
	} tx;

	struct {
		comm_frame_t * frame;

		queue_t * queue; 
	} rx;

} twi_driver_t; 

twi_driver_t * twi_init( 
			TWI_t * dev, 
			const uint8_t addr, 
			const uint8_t mask, 
			const uint8_t baud, 
			const uint8_t mtu, 
			chunkpool_t * pool );



void twi_send(twi_driver_t * twi, comm_frame_t * frame);
void twi_tx(twi_driver_t * twi);
comm_frame_t * twi_rx(twi_driver_t * twi);
void twi_master_isr(twi_driver_t * twi);
void twi_slave_isr(twi_driver_t * twi);


#endif
