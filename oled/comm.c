#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>

#include <util.h>
#include <uart.h>

#include "display.h"
#include "comm.h"

#define COMM_SPI SPID
#define COMM_PORT PORTD
#define COMM_SPI_vect SPID_INT_vect

#define COMM_PIN_SOUT 6
#define COMM_PIN_SIN 5
#define COMM_PIN_SCLK 7
#define COMM_PIN_SS 4

#define _SCLK_bm G4_PIN(COMM_PIN_SCLK)
#define _SIN_bm G4_PIN(COMM_PIN_SIN)
#define _SOUT_bm G4_PIN(COMM_PIN_SOUT)
#define _SS_bm G4_PIN(COMM_PIN_SS)

#define RX_BUFF_SIZE 40

uart_driver_t comm_uart_driver;

void dummy(void);

inline void comm_init(void) {
	
	//xmegaA, pp226.
	COMM_PORT.DIRCLR = _SCLK_bm | _SS_bm | _SIN_bm;

	//not even using, but meh.
	COMM_PORT.DIRSET = _SOUT_bm;
	COMM_PORT.OUTSET = _SOUT_bm;


	COMM_SPI.CTRL = SPI_ENABLE_bm; /*SPI_MASTER_bm=0*/
	COMM_SPI.INTCTRL = SPI_INTLVL_LO_gc;

	static ringbuf_t uart_rx_buf;
	static uint8_t uart_rx_buf_raw[RX_BUFF_SIZE]; 
	ringbuf_init(&uart_rx_buf, RX_BUFF_SIZE, uart_rx_buf_raw);

	uart_init(&comm_uart_driver, &COMM_SPI.DATA, dummy,dummy, &uart_rx_buf);

}

void dummy(void) {}

inline mpc_pkt * comm_recv(void) {
	return uart_rx(&comm_uart_driver);
}

ISR(COMM_SPI_vect) {
	uart_rx_byte(&comm_uart_driver);
}
