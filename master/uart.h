#include <mpc.h>

#ifndef UART_H
#define UART_H

#define UART_TX_QUEUE_SIZE 4
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
			TX_STATE_BUSY
		} state;
		
		uint8_t bytes;
	} tx;
} uart_driver_t; 


uart_driver_t *uart_init(USART_t *usart, uint8_t buffsize);
mpc_pkt * uart_rx(uart_driver_t * driver);

//goes in the receive ISR
void uart_rx_byte(uart_driver_t *  driver, uint8_t data);

void uart_tx(uart_driver_t * driver, const uint8_t cmd, const uint8_t size, uint8_t * data);

//goes in the DRE ISR
void usart_tx_process(uart_driver_t * driver);

#endif
