#include <avr/io.h>
#include <ringbuf.h>
#include <mpc.h>

#ifndef UART_H
#define UART_H

#ifndef UART_TX_QUEUE_SIZE
#define UART_TX_QUEUE_SIZE 4
#endif

#define UART_START_BYTE 0xFF



typedef struct {

	USART_t *usart;

	struct {
		//buffer of unprocessed bytes. 
		ringbuf_t * buf;
		
		//packet being received.
		mpc_pkt * pkt;

		//bytes received in current packet. 
		uint8_t size; 

		//running crc...
		uint8_t crc;

		enum {
			//received a start byte. 
			RX_STATE_READY,
			//waiting for a start
			RX_STATE_IDLE,
			//receiving a packet.
			RX_STATE_RECEIVE,
		} state;
	} rx;

	struct {
		struct {
			uint8_t read;
			uint8_t write;
			mpc_pkt * pkts[UART_TX_QUEUE_SIZE];
		} queue;

		enum {
			TX_STATE_IDLE,
			//beginning of a packet 
			TX_STATE_START,
			TX_STATE_TRANSMIT
		} state;
		
		uint8_t bytes;
	} tx;
} uart_driver_t; 


uart_driver_t *uart_init(USART_t *usart, uint8_t buffsize);
mpc_pkt * uart_rx(uart_driver_t * driver);

//Uart_driver_t * driver
//@TODO make this an always_inline?
#define uart_rx_byte(driver) do { uint8_t data = driver->usart->DATA; ringbuf_put(driver->rx.buf, data); } while(0)

void uart_tx(uart_driver_t * driver, const uint8_t cmd, const uint8_t size, uint8_t * data);

//goes in the DRE ISR
void usart_tx_process(uart_driver_t * driver);

#endif
