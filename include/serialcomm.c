#include <malloc.h>
#include <comm.h>
#include "serialcomm.h"

//header: [start][dest addr][pktlen][check]
#define SERIAL_FRAME_START 0xFF
#define SERIAL_FRAME_HDR_SIZE 4
#define SERIAL_FRAME_HDR_LEN_OFFSET 2
#define SERIAL_FRAME_HDR_DADDR_OFFSET 1
#define SERIAL_FRAME_HDR_CHK_OFFSET 3
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

	uint8_t addr;
} serialcomm_t;


static void begin_tx(comm_driver_t * comm);

comm_dev_t * serialcomm_init(register8_t * data, void (*tx_begin)(void), void (*tx_end)(void), uint8_t addr) {
	comm_dev_t * commdev;
	serialcomm_t * serialcomm;

	commdev = smalloc(sizeof(*commdev));
	serialcomm = smalloc(sizeof(*serialcomm));

	commdev->dev = serialcomm;
	commdev->begin_tx = begin_tx;

	serialcomm->tx_begin = tx_begin;
	serialcomm->tx_end = tx_end;
	serialcomm->data = data;
	serialcomm->tx_state = SERIAL_TX_IDLE;
	serialcomm->tx_hdr[0] = SERIAL_FRAME_START;
	serialcomm->addr = addr;

	return commdev;
}

static void begin_tx(comm_driver_t * comm) {
	serialcomm_t * dev = comm->dev->dev;
	dev->tx_hdr_byte = 0;
	dev->tx_state = SERIAL_TX_START;
	dev->tx_hdr[SERIAL_FRAME_HDR_DADDR_OFFSET] = comm->addr;
	dev->tx_hdr[SERIAL_FRAME_HDR_LEN_OFFSET] = comm_tx_frame_len(comm);
	//1 bit is masked out to ensure that the check will never be 0xFF = start byte
	dev->tx_hdr[SERIAL_FRAME_HDR_CHK_OFFSET] = (comm->addr ^ comm_tx_frame_len(comm))&0xFE;
	dev->tx_begin();
}

void serialcomm_tx_isr(comm_driver_t * comm) {
	serialcomm_t * dev = comm->dev->dev;

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
