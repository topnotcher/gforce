#include "malloc.h"
#include "serialcomm.h"

#include "FreeRTOS.h"
#include "task.h"

//header: [start][dest addr][pktlen][check]
#define SERIAL_FRAME_START 0xFF
#define SERIAL_FRAME_HDR_SIZE 4
#define SERIAL_FRAME_HDR_LEN_OFFSET 2
#define SERIAL_FRAME_HDR_DADDR_OFFSET 1
#define SERIAL_FRAME_HDR_CHK_OFFSET 3


static inline void serialcomm_rx_byte(serialcomm_t *, const uint8_t);
static void serialcomm_rx_task(void *params);

serialcomm_t *serialcomm_init(
	const serialcomm_driver driver, void *(*alloc_rx_buf)(uint8_t *),
	void (*rx_callback)(uint8_t, uint8_t *), uint8_t addr) {

	serialcomm_t *serialcomm;
	serialcomm = smalloc(sizeof(*serialcomm));

	serialcomm->driver = driver;
	serialcomm->addr = addr;

	serialcomm->rx_state = SERIAL_RX_IDLE;
	serialcomm->rx_sync_state = SERIAL_RX_SYNC_IDLE;

	// TODO: this queue size doesn't necessarily match anything
	serialcomm->tx_hdr_pool = init_mempool(SERIAL_FRAME_HDR_SIZE, 10);

	serialcomm->rx_callback = rx_callback;
	serialcomm->alloc_rx_buf = alloc_rx_buf;
	serialcomm->rx_buf = alloc_rx_buf(&serialcomm->rx_buf_size);
	xTaskCreate(serialcomm_rx_task, "serial-rx", 128, serialcomm, tskIDLE_PRIORITY + 1, (TaskHandle_t*)NULL);

	return serialcomm;
}

static void serialcomm_rx_task(void *params) {
	serialcomm_t *serialdev = params;

	while (1) {
		uint8_t data = serialdev->driver.rx_func(serialdev->driver.dev);
		serialcomm_rx_byte(serialdev, data);
	}
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

static inline void serialcomm_rx_byte(serialcomm_t *dev, const uint8_t data) {
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

	if (dev->rx_buf_offset < dev->rx_buf_size) {
		dev->rx_buf[dev->rx_buf_offset++] = data;

		// expected number of bytes received
		if (dev->rx_buf_offset >= dev->rx_size) {
			dev->rx_callback(dev->rx_buf_offset, dev->rx_buf);
			dev->rx_buf = dev->alloc_rx_buf(&dev->rx_buf_size);

			dev->rx_state = SERIAL_RX_IDLE;
		}

	// buffer overflow - drop the packet.
	} else {
		dev->rx_state = SERIAL_RX_IDLE;
	}
}
