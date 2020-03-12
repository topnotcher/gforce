#include <stdint.h>
#include <stdlib.h>
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
	struct mpc_rx_data rx_data = {
		.size = size,
		.buf = buf,
	};

	xQueueSend(driver->rx_queue, &rx_data, 0);
	driver->tx_complete(buf);
}
