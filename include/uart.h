#include <avr/io.h>
#include <ringbuf.h>
#include <mpc.h>
#include <util.h>

#ifndef UART_H
#define UART_H

#ifndef UART_TX_QUEUE_SIZE
#define UART_TX_QUEUE_SIZE 4
#endif

#define UART_START_BYTE 0xFF



typedef struct {

	void (* tx_begin)(void);
	void (* tx_end)(void);
	
	register8_t * data;

	struct {
		//buffer of unprocessed bytes 
		ringbuf_t * buf;
		
		/**
		 * Buffer the packet being received - static buffer size
		 */
		union { 
			mpc_pkt pkt;
			uint8_t pkt_raw[MPC_PKT_MAX_SIZE];
		};

		//number of bytes received in current packet. 
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

	//@TODo make this byte oriented?
	struct {
		uint8_t read;
		uint8_t write;

		union {
			mpc_pkt pkt;
			uint8_t data[MPC_PKT_MAX_SIZE];
		} queue[UART_TX_QUEUE_SIZE];

		enum {
			TX_STATE_IDLE,
			//beginning of a packet 
			TX_STATE_START,
			TX_STATE_TRANSMIT
		} state;
		
		uint8_t bytes;
	} tx;
} uart_driver_t; 


void uart_init(uart_driver_t * driver, register8_t * data, void (* tx_begin)(void), void (* tx_end)(void), ringbuf_t * rxbuf);

mpc_pkt * uart_rx(uart_driver_t * driver);

//Uart_driver_t * driver
//@TODO make this an always_inline?
#define uart_rx_byte(driver) do { uint8_t data = *((driver)->data); ringbuf_put((driver)->rx.buf, data); } while(0)

void uart_tx(uart_driver_t * driver, const uint8_t cmd, const uint8_t size, uint8_t * data);

//goes in the DRE ISR
void usart_tx_process(uart_driver_t * driver);

#endif
