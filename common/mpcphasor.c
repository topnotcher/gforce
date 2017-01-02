#include <stdbool.h>
#include <stdint.h>

#include <avr/interrupt.h>

#include "mempool.h"
#include "malloc.h"
#include "serialcomm.h"
#include "uart.h"
#include "mpc.h"
#include "mpcphasor.h"

#include "g4config.h"
#include "config.h"

#include "FreeRTOS.h"
#include "task.h"

// needed for ISRs
static uart_dev_t *phasor_uart_dev;

static void mpc_rx_phasor_task(void *);
static bool mpc_phasor_registered(mpc_driver_t *);

mpc_driver_t *mpc_phasor_init(void) {
	mpc_driver_t *mpc_driver = NULL;

	uint8_t addr;
	uint8_t addrmask;

	if (MPC_ADDR_BOARD & MPC_ADDR_MASTER) {
		addr = MPC_ADDR_MASTER;
		addrmask = MPC_ADDR_PHASOR;
	} else {
		addrmask = MPC_ADDR_MASTER;
		addr = MPC_ADDR_PHASOR;
	}

	phasor_uart_dev = uart_init(
			&PHASOR_COMM_USART, MPC_QUEUE_SIZE * 2, 24,
			PHASOR_COMM_BSEL_VALUE, PHASOR_COMM_BSCALE_VALUE
	);

	if (phasor_uart_dev && (mpc_driver = smalloc(sizeof *mpc_driver))) {

		serialcomm_driver driver = {
			.dev = phasor_uart_dev,
			.rx_func = (uint8_t (*)(void*))uart_getchar,
			.tx_func = (void (*)(void *, uint8_t *, uint8_t, void (*)(void *)))uart_write,
		};

		serialcomm_t *comm = serialcomm_init(driver, addr);

		if (comm) {
			mpc_driver->addr = addr;
			mpc_driver->ins = comm;
			mpc_driver->addrmask = addrmask;
			mpc_driver->tx = mpc_phasor_send;
			mpc_driver->registered = mpc_phasor_registered;
		} else {
			mpc_driver = NULL;
		}
	}

	return mpc_driver;
}

static bool mpc_phasor_registered(mpc_driver_t *driver) {
	if (driver) {
		xTaskCreate(mpc_rx_phasor_task, "phasor-rx", 128, driver, tskIDLE_PRIORITY + 1, (TaskHandle_t*)NULL);

		return true;
	}

	return false;
}

static void mpc_rx_phasor_task(void *_params) {
	const mpc_driver_t *const driver = _params;
	serialcomm_t *const comm = driver->ins;

	while (true) {
		struct mpc_rx_data rx_data;
		uint8_t buf_size;

		if ((rx_data.buf = driver->alloc_buf(&buf_size))) {
			rx_data.size = serialcomm_recv_frame(comm, rx_data.buf, buf_size);

			xQueueSend(driver->rx_queue, &rx_data, configTICK_RATE_HZ);
		}
	}
}

void mpc_phasor_send(mpc_driver_t *driver, const uint8_t addr, const uint8_t size, uint8_t *const buf) {
	serialcomm_send(driver->ins, addr, size, buf, driver->tx_complete);
}

PHASOR_COMM_TXC_ISR {
	uart_tx_isr(phasor_uart_dev);
}

PHASOR_COMM_RXC_ISR {
	uart_rx_isr(phasor_uart_dev);
}
