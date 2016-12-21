#include <avr/interrupt.h>

#include <stdint.h>

#include <malloc.h>
#include <comm.h>
#include <serialcomm.h>
#include "uart.h"
#include <util.h>
#include <mpc.h>
#include <mempool.h>

#include <g4config.h>
#include "config.h"
#include <phasor_comm.h>

#include <util/crc16.h>
#define mpc_crc(crc, data) _crc_ibutton_update(crc, data)

static uart_dev_t *phasor_uart_dev;

comm_driver_t *phasor_comm_init(mempool_t *mempool, uint8_t mpc_addr, void (*end_rx)(comm_driver_t *)) {
	comm_dev_t *commdev;
	phasor_uart_dev = uart_init(&PHASOR_COMM_USART, MPC_QUEUE_SIZE * 2, 24, PHASOR_COMM_BSEL_VALUE, PHASOR_COMM_BSCALE_VALUE);

	serialcomm_driver driver = {
		.dev = phasor_uart_dev,
		.rx_func = (uint8_t (*)(void*))uart_getchar,
		.tx_func = (void (*)(void *, uint8_t *, uint8_t, void (*)(void *)))uart_write,
	};

	commdev = serialcomm_init(driver, mpc_addr);
	return comm_init(commdev, mpc_addr, MPC_PKT_MAX_SIZE, mempool, end_rx, MPC_QUEUE_SIZE, MPC_QUEUE_SIZE);  // TODO
}

PHASOR_COMM_TXC_ISR {
	uart_tx_isr(phasor_uart_dev);
}

PHASOR_COMM_RXC_ISR {
	uart_rx_isr(phasor_uart_dev);
}
