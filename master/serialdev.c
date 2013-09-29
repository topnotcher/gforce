#include <malloc.h>
#include <comm.h>
#include "serialdev.h"

#define SERIAL_FRAME_HDR_SIZE 1
typedef struct {
	void (*tx_begin)(void);
	void (*tx_end)(void);

	register8_t * data;

	enum {
		SERIAL_TX_START,
		SERIAL_TX_TRANSMIT,
		SERIAL_TX_IDLE,
	} tx_state;

	uint8_t tx_hdr[SERIAL_FRAME_HDR_SIZE];
	uint8_t tx_hdr_byte;

} serialdev_t;


static void begin_tx(comm_driver_t * comm);

comm_dev_t * serialdev_init(register8_t * data, void (*tx_begin)(void), void (*tx_end)(void)) {
	comm_dev_t * commdev;
	serialdev_t * serialdev;

	commdev = smalloc(sizeof(*commdev));
	serialdev = smalloc(sizeof(*serialdev));

	commdev->dev = serialdev;
	commdev->begin_tx = begin_tx;

	serialdev->tx_begin = tx_begin;
	serialdev->tx_end = tx_end;
	serialdev->data = data;
	serialdev->tx_state = SERIAL_TX_IDLE;
	serialdev->tx_hdr[0] = 0xFF;

	return commdev;	
}

static void begin_tx(comm_driver_t * comm) {
	serialdev_t * dev = comm->dev->dev;
	dev->tx_hdr_byte = 0;
	dev->tx_state = SERIAL_TX_START;
	dev->tx_begin();
}

void serialdev_tx_isr(comm_driver_t * comm) {
	serialdev_t * dev = comm->dev->dev;

	if ( dev->tx_state == SERIAL_TX_START ) { 
		*(dev->data) = dev->tx_hdr[dev->tx_hdr_byte++];
		
		if ( dev->tx_hdr_byte == SERIAL_FRAME_HDR_SIZE )
			dev->tx_state = SERIAL_TX_TRANSMIT;
	} else if ( dev->tx_state == SERIAL_TX_TRANSMIT ) {
		if ( comm_tx_has_more(comm) ) {
			(*dev->data) = comm_tx_next(comm);
		} else {
			dev->tx_end();
			comm_end_tx(comm);
			dev->tx_state = SERIAL_TX_IDLE;
		}
 	}
}
