#include <stddef.h>

#include "comm.h"
#include "queue.h"
#include <malloc.h>

comm_driver_t * comm_init( comm_dev_t * dev, const uint8_t addr, const uint8_t mtu, chunkpool_t * pool )  {

	comm_driver_t * comm;
	comm = smalloc(sizeof *comm);

	dev->comm = comm;
	comm->dev = dev;

	comm->addr = addr;
	comm->mtu = mtu;
	comm->pool = pool;

	comm->rx.queue = queue_create(4);
	comm->rx.frame = NULL;

	comm->tx.state = COMM_TX_STATE_IDLE;
	comm->tx.queue = queue_create(4);
	comm->tx.frame = NULL;

	return comm;
}

/**
 * note: this assumes that * frame must have been allocated by the chunk pool allocator
 */
void comm_send(comm_driver_t * comm, comm_frame_t * frame) {
	//@todo reture failure?
	queue_offer( comm->tx.queue, (void*)frame );
	chunkpool_incref(frame);
}

/**
 * task to initiate transfers
 */
void comm_tx(comm_driver_t * comm) {
	comm_frame_t * frame;
	
	if ( comm->tx.state != COMM_TX_STATE_IDLE ) 
		return;

	if ( (frame = queue_poll(comm->tx.queue)) != NULL ) {
		//safe to modify and idle status indicates that the entire tx state is safe to modify.
		comm->tx.state = COMM_TX_STATE_BUSY;
		comm->tx.frame = frame;
		comm->tx.byte = 0;
		comm->dev->begin_tx(comm);
	}
}

comm_frame_t * comm_rx(comm_driver_t * comm) {
	return queue_poll(comm->rx.queue);
}

void comm_end_tx(comm_driver_t * comm) {
	if (comm->tx.frame != NULL) {
		chunkpool_decref(comm->tx.frame);
		comm->tx.frame = NULL;
	}		
		
	comm->tx.state = COMM_TX_STATE_IDLE;
}

void comm_begin_rx(comm_driver_t * comm) {
	//just in case - but if it is not-null, then there's an
	//in complete transaction. it's probably garbage, so drop it.
	//@TODO acquire may fail.
	if ( comm->rx.frame == NULL ) 
		comm->rx.frame = (comm_frame_t*)chunkpool_acquire(comm->pool);
				
	comm->rx.frame->size = 0;
}

void comm_end_rx(comm_driver_t * comm) {
	queue_offer(comm->rx.queue, (void*)comm->rx.frame);	
	comm->rx.frame = NULL;
}

