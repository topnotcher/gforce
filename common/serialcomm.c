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

	register8_t *data;

	enum {
		SERIAL_TX_START,
		SERIAL_TX_TRANSMIT,
		SERIAL_TX_IDLE,
	} tx_state;

	uint8_t tx_hdr[SERIAL_FRAME_HDR_SIZE];
	uint8_t tx_hdr_byte;

	enum {
		SERIAL_RX_SYNC_IDLE,
		SERIAL_RX_SYNC_DADDR,
		SERIAL_RX_SYNC_SIZE,
		SERIAL_RX_SYNC_CHK
	} rx_sync_state;

	enum {
		SERIAL_RX_IDLE,
		SERIAL_RX_RECV,
	} rx_state;

	uint8_t rx_sync_chk;
	uint8_t rx_sync_size;

	uint8_t rx_size;

	uint8_t addr;
} serialcomm_t;


static void begin_tx(comm_driver_t *comm);

comm_dev_t *serialcomm_init(register8_t *data, void (*tx_begin)(void), void (*tx_end)(void), uint8_t addr) {
	comm_dev_t *commdev;
	serialcomm_t *serialcomm;

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

	serialcomm->rx_state = SERIAL_RX_IDLE;
	serialcomm->rx_sync_state = SERIAL_RX_SYNC_IDLE;

	return commdev;
}

static void begin_tx(comm_driver_t *comm) {
	serialcomm_t *dev = comm->dev->dev;
	dev->tx_hdr_byte = 0;
	dev->tx_state = SERIAL_TX_START;
	dev->tx_hdr[SERIAL_FRAME_HDR_DADDR_OFFSET] = comm_tx_daddr(comm);
	dev->tx_hdr[SERIAL_FRAME_HDR_LEN_OFFSET] = comm_tx_len(comm);
	//1 bit is masked out to ensure that the check will never be 0xFF = start byte
	dev->tx_hdr[SERIAL_FRAME_HDR_CHK_OFFSET] = (comm_tx_daddr(comm) ^ comm_tx_len(comm)) & 0xFE;
	dev->tx_begin();
}

void serialcomm_tx_isr(comm_driver_t *comm) {
	serialcomm_t *dev = comm->dev->dev;

	if (dev->tx_state == SERIAL_TX_START) {
		*(dev->data) = dev->tx_hdr[dev->tx_hdr_byte++];
		if (dev->tx_hdr_byte == SERIAL_FRAME_HDR_SIZE)
			dev->tx_state = SERIAL_TX_TRANSMIT;
	} else if (dev->tx_state == SERIAL_TX_TRANSMIT) {
		if (comm_tx_has_more(comm)) {
			(*dev->data) = comm_tx_next(comm);
		} else {
			dev->tx_end();
			dev->tx_state = SERIAL_TX_IDLE;

			//NOTE: This could cause execution to reenter this function.
			//(when there is another packet pending)
			comm_end_tx(comm);
		}
	}
}

void serialcomm_rx_isr(comm_driver_t *comm) {
	serialcomm_t *dev = comm->dev->dev;
	uint8_t data = *dev->data;

	if (data == SERIAL_FRAME_START) {
		dev->rx_sync_state = SERIAL_RX_SYNC_DADDR;
	} else {
		switch (dev->rx_sync_state) {
		case SERIAL_RX_SYNC_DADDR:
			if (data & dev->addr) {
				dev->rx_sync_chk = data;
				dev->rx_sync_state = SERIAL_RX_SYNC_SIZE;
			} else {
				dev->rx_sync_state = SERIAL_RX_SYNC_IDLE;
			}
			break;

		case SERIAL_RX_SYNC_SIZE:
			dev->rx_sync_state = SERIAL_RX_SYNC_CHK;
			dev->rx_sync_size = data;
			dev->rx_sync_chk ^= data;
			dev->rx_sync_chk &= 0xFE;
			break;

		case SERIAL_RX_SYNC_CHK:
			dev->rx_sync_state = SERIAL_RX_SYNC_IDLE;

			if (data == dev->rx_sync_chk) {
				if (dev->rx_state == SERIAL_RX_IDLE) {
					dev->rx_state = SERIAL_RX_RECV;
				} else {
					comm_end_rx(comm);
				}

				dev->rx_size = dev->rx_sync_size;
				comm_begin_rx(comm);
				return;
			}

			break;

		case SERIAL_RX_SYNC_IDLE:
		default:
			break;
		}
	}

	if (dev->rx_state != SERIAL_RX_RECV)
		return;

	comm_rx_byte(comm, data);

	if (comm_rx_full(comm) || (comm_rx_size(comm) >= dev->rx_size)) {
		comm_end_rx(comm);
		dev->rx_state = SERIAL_RX_IDLE;
	}
}
