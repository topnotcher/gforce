#include <stdint.h>

#include "mempool.h"
#include "gqueue.h"

#ifndef COMM_H
#define COMM_H

typedef struct {
	uint8_t daddr;
	uint8_t size;
	uint8_t data[];
} comm_frame_t;

//forward definition.
struct comm_dev_struct; 
struct comm_driver_struct;

typedef struct comm_driver_struct {

	struct comm_dev_struct * dev;
	
	void (*end_rx)(struct comm_driver_struct *);

	mempool_t * pool;

	uint8_t mtu;
	uint8_t addr;

	struct {
		enum {
			COMM_TX_STATE_IDLE = 0,
			COMM_TX_STATE_BUSY = 1
		} state;

		queue_t * queue; 

		comm_frame_t * frame;
		uint8_t byte;
	} tx;

	struct {
		comm_frame_t * frame;
		queue_t * queue; 
	} rx;

} comm_driver_t; 

typedef struct comm_dev_struct {

	//pointer to some device-specific structure.
	void * dev;

	comm_driver_t * comm;

	void (* begin_tx)(comm_driver_t *);
} comm_dev_t;

comm_driver_t * comm_init( comm_dev_t * dev, const uint8_t addr, const uint8_t mtu, mempool_t * pool, void (*end_rx)(comm_driver_t*) );
void comm_tx(comm_driver_t * comm);
comm_frame_t * comm_rx(comm_driver_t * comm);
void comm_end_tx(comm_driver_t * comm);
void comm_begin_rx(comm_driver_t * comm);
void comm_end_rx(comm_driver_t * comm);
void comm_rx_end(comm_driver_t * comm, uint8_t bytes);
void comm_send(comm_driver_t * comm, comm_frame_t * frame);

/**
 * Called by the lower-level device driver to determine
 * if there are more bytes left to send in the current transfer
 */
#define comm_tx_has_more(comm) ((comm)->tx.byte < (comm)->tx.frame->size)

/**
 * Called by the lower-level device driver to determine
 * the size of the frame currently being sent
 */
#define comm_tx_len(comm) ((comm)->tx.frame->size)

#define comm_tx_data(comm) ((comm)->tx.frame->data)

/**
 * Called bythe lower-level driver to retreive the next
 * byte in the current transfer.
 */
#define comm_tx_next(comm) ((comm)->tx.frame->data[(comm)->tx.byte++])

/**
 * Called by the lower-level driver to determine the destination
 * address of the current transfer.
 */
#define comm_tx_daddr(comm) ((comm)->tx.frame->daddr)

/**
 * Called by the lower level driver to restart the current transfer
 * e.g. in case of a receoverable error, try resending the frame
 */
#define comm_tx_rewind(comm) ((comm)->tx.byte = 0)

/**
 * Called by the lower level driver when a byte is received
 */
#define comm_rx_byte(comm,byte) (((comm)->rx.frame->data[(comm)->rx.frame->size++]) = (byte))

/**
 * Called by the lower level driver to check if the RX buffer is full
 */
#define comm_rx_full(comm) ((comm)->rx.frame->size >= (comm)->mtu)

/**
 * Called by the lower level driver to get the numer of bytes received in the current frame
 */
#define comm_rx_size(comm)  ((comm)->rx.frame->size)

#define comm_rx_buf(comm) ((comm)->rx.frame->data)

#define comm_rx_max_size(comm) ((comm)->mtu)
#endif
