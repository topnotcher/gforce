#include <avr/io.h>
#include <util/crc16.h>
#include <stddef.h>
#include <stdlib.h>

#include <g4config.h>
#include "config.h"

#include <util.h>
#include <ringbuf.h>
#include <mpc.h>

#include <uart.h>

#include <malloc.h>

#define uart_crc(crc,data) _crc_ibutton_update(crc,data)

//return true if more processing is required before returning a packet.
static inline uint8_t uart_process_byte(uart_driver_t *driver, uint8_t data)  ATTR_ALWAYS_INLINE;

uart_driver_t * uart_init(register8_t * data, void (* tx_begin)(void), void (* tx_end)(void), const uint8_t rxbuffsize) {
	uart_driver_t * driver;

	driver = smalloc(sizeof *driver); 

	driver->tx_begin = tx_begin;
	driver->tx_end = tx_end;
	driver->data = data;

	driver->rx.buf = ringbuf_init(rxbuffsize);

	driver->rx.size = 0;
	driver->rx.state = RX_STATE_IDLE;

	driver->tx.state = TX_STATE_IDLE;
	driver->tx.read = 0;
	driver->tx.write = 0;
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
		return &driver->rx.pkt;

	return NULL;
}


static inline uint8_t uart_process_byte(uart_driver_t * driver, uint8_t data) {

	if ( driver->rx.state == RX_STATE_IDLE ) {
		
		if ( data == UART_START_BYTE ) 
			driver->rx.state = RX_STATE_READY;

	} else if ( driver->rx.state == RX_STATE_READY ) {

		driver->rx.crc = uart_crc(MPC_CRC_SHIFT, data);
		driver->rx.pkt.len = data;
		driver->rx.size = 1;
		driver->rx.state = RX_STATE_RECEIVE;

	} else if ( driver->rx.state == RX_STATE_RECEIVE ) {
		driver->rx.pkt_raw[driver->rx.size] = data;

		if ( driver->rx.size != offsetof(mpc_pkt,chksum) )
			driver->rx.crc = uart_crc(driver->rx.crc, data);

		//wtf does sizeof rx.pkt =??? => it's the ACTUAL size of the mpc_pkt_t, not the size of the union
		if ( ++driver->rx.size == driver->rx.pkt.len+sizeof(driver->rx.pkt) ) {

			driver->rx.state = RX_STATE_IDLE;

			if ( driver->rx.crc == driver->rx.pkt.chksum ) 
				return 0;
		}
	}

	//packet not done = true
	return 1;
}

//so  ttly copied from mpc :/
void uart_tx(uart_driver_t * driver, const uint8_t cmd, const uint8_t size, uint8_t * data) {

	mpc_pkt * pkt = &driver->tx.queue[driver->tx.write].pkt;

	pkt->len = size;
	pkt->cmd = cmd;
	pkt->saddr = MPC_ADDR<<1; //@TODO
	pkt->chksum = MPC_CRC_SHIFT;

	for ( uint8_t i = 0; i < sizeof(*pkt)-sizeof(pkt->chksum); ++i )
		pkt->chksum = uart_crc(pkt->chksum, driver->tx.queue[driver->tx.write].data[i]);

	for ( uint8_t i = 0; i < size; ++i ) {
			driver->tx.queue[driver->tx.write].data[sizeof(*pkt)+i] = data[i];
			pkt->chksum = uart_crc(pkt->chksum, data[i]);
	}
	
	driver->tx.write = ( driver->tx.write == UART_TX_QUEUE_SIZE - 1 ) ? 0 : driver->tx.write + 1;

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

	mpc_pkt * pkt = &driver->tx.queue[driver->tx.read].pkt;

	if ( driver->tx.bytes < (sizeof(mpc_pkt) + pkt->len) ) {

		//mpc_pkt * pkt = driver->tx.queue.pkts[driver->tx.queue.read];
		*(driver->data) = driver->tx.queue[driver->tx.read].data[driver->tx.bytes];
		++driver->tx.bytes;

	//last byte finished sending.
	} else {
		driver->tx.read = ( driver->tx.read == UART_TX_QUEUE_SIZE - 1 ) ? 0 : driver->tx.read + 1;

		//no more packets to send.
		if ( driver->tx.read == driver->tx.write ) {
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
