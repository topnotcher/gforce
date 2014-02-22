#include <stddef.h>

#include "comm.h"
#include "queue.h"
#include <malloc.h>

/**
 * The comm driver represents a generalized device 
 * for sending and receiving packets (comm_frame_t).
 *
 * This was primarly an effort to combine the similarities 
 * between all of my comm code into one place. Formerly,
 * all of the individual device drivers implemented similar
 * code to send full packets, and to group received bytes
 * into packets. 
 *
 * The driver below this (comm_dev_t) *IS* expected to be
 * able to detect the beginning and end of a packet, and notify
 * this driver when it does. All of the per-packet buffers and queues
 * are maintained in this driver.
 *
 * On the *TX* side, the driver is expected to have some concept of 
 * sending a "group" of bytes.
 */

comm_driver_t * comm_init( comm_dev_t * dev, const uint8_t addr, const uint8_t mtu, chunkpool_t * pool, void (*end_rx)(comm_driver_t*) ) {

	comm_driver_t * comm;
	comm = smalloc(sizeof *comm);

	dev->comm = comm;
	comm->dev = dev;

	comm->addr = addr;
	comm->mtu = mtu;
	comm->pool = pool;
	comm->end_rx = end_rx;

	/**
	 * @TODO hard-coded queue sizes
	 */
	comm->rx.queue = queue_create(4);
	comm->rx.frame = NULL;

	comm->tx.state = COMM_TX_STATE_IDLE;
	comm->tx.queue = queue_create(4);
	comm->tx.frame = NULL;

	return comm;
}

/**
 * note: this assumes that * frame must have been 
 * allocated by the chunk pool allocator
 */
void comm_send(comm_driver_t * comm, comm_frame_t * frame) {
	//@todo reture failure?
	queue_offer( comm->tx.queue, (void*)frame );
	chunkpool_incref(frame);
}

/**
 * task to initiate transfers. Intended to be run in the main loop.
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

/**
 * Called by the lower-level device driver when 
 * all bytes of the transfer have been sent out.
 */
void comm_end_tx(comm_driver_t * comm) {
	if (comm->tx.frame != NULL) {
		chunkpool_decref(comm->tx.frame);
		comm->tx.frame = NULL;
	}		
		
	comm->tx.state = COMM_TX_STATE_IDLE;
}

/**
 * Called by the lower-level device driver when 
 * the beginning of a new incoming frame is detected. 
 */
void comm_begin_rx(comm_driver_t * comm) {
	/**
	 * In the case where the frame is non-NULL, there is a 
	 * partially-received frame still in the buffer.  Chances are
	 * that if the end wasn't detected, something went wrong, so 
	 * we might as well just drop the whole frame.
	 */
	if ( comm->rx.frame == NULL ) 
		comm->rx.frame = (comm_frame_t*)chunkpool_acquire(comm->pool);
				
	comm->rx.frame->size = 0;
}

/**
 * Called by the lower-level device driver when 
 * the end of an incoming frame is detected. 
 */
void comm_end_rx(comm_driver_t * comm) {
	queue_offer(comm->rx.queue, (void*)comm->rx.frame);	
	comm->rx.frame = NULL;
}

void comm_rx_end(comm_driver_t * comm, uint8_t bytes) {
	comm->rx.frame->size = bytes;
	queue_offer(comm->rx.queue, (void*)comm->rx.frame);	
	comm->rx.frame = NULL;
	if (comm->end_rx)
		comm->end_rx(comm);
}
