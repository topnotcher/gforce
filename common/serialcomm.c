#include "mempool.h"
#include "malloc.h"
#include "serialcomm.h"

//header: [start][dest addr][pktlen][check]
#define SERIAL_FRAME_START 0xFF
#define SERIAL_FRAME_HDR_SIZE 4
#define SERIAL_FRAME_HDR_LEN_OFFSET 2
#define SERIAL_FRAME_HDR_DADDR_OFFSET 1
#define SERIAL_FRAME_HDR_CHK_OFFSET 3

static inline uint8_t serialcomm_rx_byte(serialcomm_t *, const uint8_t);

serialcomm_t *serialcomm_init(const serialcomm_driver driver, const uint8_t addr) {
	serialcomm_t *serialcomm;
	serialcomm = smalloc(sizeof(*serialcomm));

	if (serialcomm) {
		serialcomm->driver = driver;
		serialcomm->addr = addr;

		serialcomm->rx_state = SERIAL_RX_IDLE;
		serialcomm->rx_sync_state = SERIAL_RX_SYNC_IDLE;

		// TODO: this queue size doesn't necessarily match anything
		serialcomm->tx_hdr_pool = init_mempool(SERIAL_FRAME_HDR_SIZE, 10);
		serialcomm->rx_buf = NULL;
	}

	return serialcomm;
}

uint8_t serialcomm_recv_frame(serialcomm_t *dev, uint8_t *buf, uint8_t size) {
	dev->rx_buf = buf;
	dev->rx_buf_size = size;

	uint8_t rx_pending = 1;

	while (rx_pending) {
		uint8_t data = dev->driver.rx_func(dev->driver.dev);
		rx_pending = serialcomm_rx_byte(dev, data);
	}
	
	dev->rx_buf = NULL;
	return dev->rx_size;
}

void serialcomm_send(serialcomm_t *dev, uint8_t daddr, const uint8_t size, uint8_t *buf, void (*complete)(void *)) {
	uint8_t *hdr = mempool_alloc(dev->tx_hdr_pool);

	if (hdr) {
		hdr[0] = SERIAL_FRAME_START;
		hdr[SERIAL_FRAME_HDR_DADDR_OFFSET] = daddr;
		hdr[SERIAL_FRAME_HDR_LEN_OFFSET] = size;
		//1 bit is masked out to ensure that the check will never be 0xFF = start byte
		hdr[SERIAL_FRAME_HDR_CHK_OFFSET] = (daddr ^ size) & 0xFE;

		dev->driver.tx_func(dev->driver.dev, hdr, SERIAL_FRAME_HDR_SIZE, mempool_putref);
		dev->driver.tx_func(dev->driver.dev, buf, size, complete);
	} else {
		complete(buf);
	}
}

static inline uint8_t serialcomm_rx_byte(serialcomm_t *dev, const uint8_t data) {
	uint8_t rx_pending = 1;

	if (data == SERIAL_FRAME_START) {
		dev->rx_sync_state = SERIAL_RX_SYNC_DADDR;
	} else {
		switch (dev->rx_sync_state) {
		case SERIAL_RX_SYNC_DADDR:
			if (data & dev->addr) {
				dev->rx_chk = data;
				dev->rx_sync_state = SERIAL_RX_SYNC_SIZE;
			} else {
				dev->rx_sync_state = SERIAL_RX_SYNC_IDLE;
			}
			break;

		case SERIAL_RX_SYNC_SIZE:
			dev->rx_sync_state = SERIAL_RX_SYNC_CHK;
			dev->rx_size = data;
			dev->rx_chk ^= data;
			dev->rx_chk &= 0xFE;
			break;

		case SERIAL_RX_SYNC_CHK:
			dev->rx_sync_state = SERIAL_RX_SYNC_IDLE;

			if (data == dev->rx_chk) {
				dev->rx_state = SERIAL_RX_RECV;
				dev->rx_buf_offset = 0;

				goto out;
			}

			break;

		case SERIAL_RX_SYNC_IDLE:
		default:
			break;
		}
	}

	if (dev->rx_state != SERIAL_RX_RECV)
		goto out;

	if (dev->rx_buf != NULL && dev->rx_buf_offset < dev->rx_buf_size) {
		dev->rx_buf[dev->rx_buf_offset++] = data;

		// expected number of bytes received
		if (dev->rx_buf_offset == dev->rx_size) {
			rx_pending = 0;
			dev->rx_state = SERIAL_RX_IDLE;
		}

	// buffer overflow - drop the packet.
	} else {
		dev->rx_state = SERIAL_RX_IDLE;
	}

out:
	return rx_pending;
}
