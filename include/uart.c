#include <avr/io.h>
#include <util/crc16.h>
#include <stddef.h>
#include <stdlib.h>

#include <g4config.h>
#include "config.h"

#include <ringbuf.h>
#include <mpc.h>

#include <uart.h>

#define uart_crc(crc,data) _crc_ibutton_update(crc,data)

//return true if more processing is required before returning a packet.
static inline uint8_t uart_process_byte(uart_driver_t *driver, uint8_t data);

uart_driver_t *uart_init(register8_t * data, void (* tx_begin)(void), void (* tx_end)(void), uint8_t buffsize) {
	uart_driver_t * driver = malloc(sizeof *driver);

	driver->tx_begin = tx_begin;
	driver->tx_end = tx_end;
	driver->data = data;
	driver->rx.buf = ringbuf_init(buffsize);
	driver->rx.size = 0;
	driver->rx.state = RX_STATE_IDLE;

	driver->tx.state = TX_STATE_IDLE;
	driver->tx.queue.read = 0;
	driver->tx.queue.write = 0;
	driver->tx.bytes = 0;

	return driver;
}

mpc_pkt * uart_rx(uart_driver_t * driver) {

	//@TODO
	const uint8_t process_max = 10;
	uint8_t i = 0;
	uint8_t more = 1;
	while ( !ringbuf_empty(driver->rx.buf) && more && i++ < process_max ) 
		more = uart_process_byte(driver, ringbuf_get(driver->rx.buf));

	if ( !more ) 
		return driver->rx.pkt;

	return NULL;
}


static inline uint8_t uart_process_byte(uart_driver_t * driver, uint8_t data) {

	if ( driver->rx.state == RX_STATE_IDLE ) {
		
		if ( data == UART_START_BYTE ) 
			driver->rx.state = RX_STATE_READY;

	} else if ( driver->rx.state == RX_STATE_READY ) {

		driver->rx.pkt = malloc(sizeof(*driver->rx.pkt) + data);
		driver->rx.crc = uart_crc(MPC_CRC_SHIFT, data);
		driver->rx.pkt->len = data;
		driver->rx.size = 1;
		driver->rx.state = RX_STATE_RECEIVE;

	} else if ( driver->rx.state == RX_STATE_RECEIVE ) {
		((uint8_t*)driver->rx.pkt)[driver->rx.size] = data;

		if ( driver->rx.size != offsetof(mpc_pkt,chksum) )
			driver->rx.crc = uart_crc(driver->rx.crc, data);


		if ( ++driver->rx.size == driver->rx.pkt->len+sizeof(*driver->rx.pkt) ) {	

			driver->rx.state = RX_STATE_IDLE;

			if ( driver->rx.crc == driver->rx.pkt->chksum ) 
				return 0;
			else 
				free(driver->rx.pkt);
		}
	}

	//packet not done = true
	return 1;
}

//so  ttly copied from mpc :/
void uart_tx(uart_driver_t * driver, const uint8_t cmd, const uint8_t size, uint8_t * data) {

	mpc_pkt * pkt = (mpc_pkt*)malloc(sizeof(*pkt) + size);

	pkt->len = size;
	pkt->cmd = cmd;
	pkt->saddr = MPC_TWI_ADDR<<1; //@TODO
	pkt->chksum = MPC_CRC_SHIFT;

	for ( uint8_t i = 0; i < sizeof(*pkt)-sizeof(pkt->chksum); ++i )
		pkt->chksum = uart_crc(pkt->chksum, ((uint8_t*)pkt)[i]);

	for ( uint8_t i = 0; i < size; ++i ) {
			pkt->data[i] = data[i];
			pkt->chksum = uart_crc(pkt->chksum, data[i]);
	}
	
	driver->tx.queue.pkts[ driver->tx.queue.write ] = pkt;
	driver->tx.queue.write = ( driver->tx.queue.write == UART_TX_QUEUE_SIZE - 1 ) ? 0 : driver->tx.queue.write + 1;

	if ( driver->tx.state == TX_STATE_IDLE ) {
		driver->tx.state = TX_STATE_START;
		driver->tx_begin();
	}
}

inline void usart_tx_process(uart_driver_t * driver) {

	//first byte of transfer
start:
	if ( driver->tx.state == TX_STATE_START ) { 
		*(driver->data) = 0xFF;
		driver->tx.state = TX_STATE_TRANSMIT;
		driver->tx.bytes = 0;
		return;
	//not completely sent.
	}

	mpc_pkt * pkt = driver->tx.queue.pkts[driver->tx.queue.read];

	if ( driver->tx.bytes < (sizeof(mpc_pkt) + pkt->len) ) {

		//mpc_pkt * pkt = driver->tx.queue.pkts[driver->tx.queue.read];
		*(driver->data) = ((uint8_t*)pkt)[driver->tx.bytes];
		++driver->tx.bytes;

	//last byte finished sending.
	} else {
		driver->tx.queue.read = ( driver->tx.queue.read == UART_TX_QUEUE_SIZE - 1 ) ? 0 : driver->tx.queue.read + 1;

		//no more packets to send.
		if ( driver->tx.queue.read == driver->tx.queue.write ) {
			driver->tx_end();
			driver->tx.state = TX_STATE_IDLE;
		} else {
			driver->tx.state = TX_STATE_START;
			//@TODO with USART, this is DRE int = we should send a byte NOW.
			//with SPI,
			goto start;
		}
	}
}
