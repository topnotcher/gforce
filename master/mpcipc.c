#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <malloc.h>

#include "mpcipc.h"

mpc_driver_t *mpc_loopback_init(uint8_t addr, uint8_t addrmask) {
	mpc_driver_t *driver = smalloc(sizeof *driver);

	if (driver) {
		driver->ins = NULL;
		driver->addr = addr;
		driver->addrmask = addrmask;
		driver->tx = mpc_loopback_send;
		driver->registered = NULL;
	}

	return driver;
}

void mpc_loopback_send(mpc_driver_t *driver, const uint8_t addr, const uint8_t size, uint8_t *const buf) {

	// We need to copy this to an RX buffer because tx_complete() will free buf.[
	uint8_t buf_size = 0;
	uint8_t *tx_buf = driver->alloc_buf(&buf_size);
	uint8_t tx_size = (size > buf_size) ? buf_size : size;
	memcpy(tx_buf, buf, tx_size);

	struct mpc_rx_data rx_data = {
		.size = tx_size,
		.buf = tx_buf,
	};

	xQueueSend(driver->rx_queue, &rx_data, 0);
	driver->tx_complete(buf);
}
