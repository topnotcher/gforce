#include <stdint.h>

#include "chunkpool.h"
#include "queue.h"

#ifndef COMM_H
#define COMM_H

typedef struct {
	uint8_t daddr;
	uint8_t size;
	uint8_t data[];
} comm_frame_t;

//forward definition.
struct comm_dev_struct; 

typedef struct {

	struct comm_dev_struct * dev;

	chunkpool_t * pool;

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

comm_driver_t * comm_init( comm_dev_t * dev, const uint8_t addr, const uint8_t mtu, chunkpool_t * pool );
void comm_tx(comm_driver_t * comm);
comm_frame_t * comm_rx(comm_driver_t * comm);
void comm_end_tx(comm_driver_t * comm);
void comm_begin_rx(comm_driver_t * comm);
void comm_end_rx(comm_driver_t * comm);
void comm_send(comm_driver_t * comm, comm_frame_t * frame);

#define comm_tx_has_more(comm) ((comm)->tx.byte < (comm)->tx.frame->size)
#define comm_tx_next(comm) ((comm)->tx.frame->data[(comm)->tx.byte++])
#define comm_tx_daddr(comm) ((comm)->tx.frame->daddr)
#define comm_tx_rewind(comm) ((comm)->tx.byte = 0)
#define comm_rx_byte(comm,byte) (((comm)->rx.frame->data[(comm)->rx.frame->size++]) = (byte))
#define comm_rx_full(comm) ((comm)->rx.frame->size >= (comm)->mtu)
#endif
