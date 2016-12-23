#include "malloc.h"
#include "comm.h"
#include "serialcomm.h"

#include "FreeRTOS.h"
#include "task.h"

//header: [start][dest addr][pktlen][check]
#define SERIAL_FRAME_START 0xFF
#define SERIAL_FRAME_HDR_SIZE 4
#define SERIAL_FRAME_HDR_LEN_OFFSET 2
#define SERIAL_FRAME_HDR_DADDR_OFFSET 1
#define SERIAL_FRAME_HDR_CHK_OFFSET 3


typedef struct {
	serialcomm_driver driver;

	mempool_t *tx_hdr_pool;

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


static inline void serialcomm_rx_byte(comm_driver_t *comm, const uint8_t data);
static void serialcomm_rx_task(void *params);

comm_dev_t *serialcomm_init(const serialcomm_driver driver, uint8_t addr) {
	comm_dev_t *commdev;
	serialcomm_t *serialcomm;

	commdev = smalloc(sizeof(*commdev));
	serialcomm = smalloc(sizeof(*serialcomm));

	commdev->comm = NULL;
	commdev->dev = serialcomm;

	serialcomm->driver = driver;
	serialcomm->addr = addr;

	serialcomm->rx_state = SERIAL_RX_IDLE;
	serialcomm->rx_sync_state = SERIAL_RX_SYNC_IDLE;

	// TODO: this queue size doesn't necessarily match anything
	serialcomm->tx_hdr_pool = init_mempool(SERIAL_FRAME_HDR_SIZE, 10);
	xTaskCreate(serialcomm_rx_task, "serial-rx", 128, commdev, tskIDLE_PRIORITY + 1, (TaskHandle_t*)NULL);

	return commdev;
}

static void serialcomm_rx_task(void *params) {
	comm_dev_t *commdev = params;
	serialcomm_t *serialdev  = commdev->dev;

	while (1) {
		uint8_t data = serialdev->driver.rx_func(serialdev->driver.dev);

		// why?
		if (commdev->comm)
			serialcomm_rx_byte(commdev->comm, data);
	}
}

void serialcomm_send(comm_driver_t *comm, uint8_t daddr, const uint8_t size, uint8_t *buf, void (*complete)(void *)) {
	serialcomm_t *dev = comm->dev->dev;

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

static inline void serialcomm_rx_byte(comm_driver_t *comm, const uint8_t data) {
	serialcomm_t *dev = comm->dev->dev;

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
