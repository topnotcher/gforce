#include <avr/interrupt.h>

#include <stdint.h>

#include <uart.h>
#include <util.h>

#include "config.h"
#include <phasor_comm.h>

#define _TXPIN_bm G4_PIN(PHASOR_COMM_USART_TXPIN)
#define _RXPIN_bm G4_PIN(PHASOR_COMM_USART_RXPIN)

#define PHASOR_COMM_QUEUE_MAX 5

static uart_driver_t * comm_uart_driver;

inline void phasor_comm_init(void) {

	PHASOR_COMM_USART.BAUDCTRLA = (uint8_t)( PHASOR_COMM_BSEL_VALUE & 0x00FF );
	PHASOR_COMM_USART.BAUDCTRLB = (PHASOR_COMM_BSCALE_VALUE<<USART_BSCALE_gp) | (uint8_t)( (PHASOR_COMM_BSEL_VALUE>>8) & 0x0F ) ;

	PHASOR_COMM_USART.CTRLC |= USART_PMODE_DISABLED_gc | USART_CHSIZE_8BIT_gc;
	PHASOR_COMM_USART.CTRLA |= USART_RXCINTLVL_MED_gc;
	PHASOR_COMM_USART.CTRLB |= USART_RXEN_bm | USART_TXEN_bm;

	//xmegaA, p237
	PHASOR_COMM_USART_PORT.OUTSET = _TXPIN_bm;
	PHASOR_COMM_USART_PORT.DIRSET = _TXPIN_bm;

	comm_uart_driver = uart_init(&PHASOR_COMM_USART, PHASOR_COMM_QUEUE_MAX);
}

inline void phasor_comm_send(const uint8_t cmd, uint8_t * data, const uint8_t size) {
	uart_tx(comm_uart_driver, cmd, size, data);
}

inline mpc_pkt * phasor_comm_recv(void) {
	return uart_rx(comm_uart_driver);
}

PHASOR_COMM_TXC_ISR {
	usart_tx_process(comm_uart_driver);
}

PHASOR_COMM_RXC_ISR {
	uart_rx_byte(comm_uart_driver);
}
