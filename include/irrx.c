#include <stdlib.h>
#include <stddef.h>

#include <avr/io.h>
#include <avr/interrupt.h>

#include <util.h>
#include "config.h"
#include "g4config.h"
#include <mpc.h>
#include <irrx.h>
#include <leds.h>
#include <ringbuf.h>

#define _RXPIN_bm G4_PIN(IRRX_USART_RXPIN)

#define N_BUFF 4
#define RX_BUFF_SIZE 5
#define MAX_PKT_SIZE 15

//when the packet timer reaches this level, set the state to process.
#define RX_TIMEOUT_TICKS 10

/**
 * In order to keep the processing out of the ISR 
 * and avoid storing uint16_t, the 9th bit is read in the ISR.
 * It could be used to clear the state of the receiver, but if that 
 * happens while a packet is still processing, good data is droppedd.
 * Instead, the ISR will begin writing to a new queue.
 * (This is probably very unnecessary given the bit rate versus CPU speed)
 */
static struct {
	
	/**
	 * Queues of unprocessed bytes.
	 */
	uint8_t write;
	uint8_t read;

	struct {
		enum {
			//Dirty state 
			RX_STATE_EMPTY,
			//ready to receive data
			RX_STATE_READY,
			//receiving data 
			RX_STATE_RECEIVE,
			//no more data will be received for this pkt,
			//so finish processing remaining bytes then return pkt.
			RX_STATE_PROCESS,
		} state ;

		ringbuf_t * buf;
		uint8_t crc;

		//size at which to process.
		uint8_t max_size;
		
		//incremented by a timer :p.
		uint8_t timer; 
		//copied to an ir_pkt_t after processing. 
		uint8_t size;
		uint8_t * data;

	} pkts[N_BUFF];
} rx_state;


static inline void process_rx_byte(void);
static void rx_timer_tick(void);

inline void irrx_init(void) {

	rx_state.read = 0;
	rx_state.write = 0;
	
	for (uint8_t i = 0; i < N_BUFF; ++i) {
		rx_state.pkts[i].buf = ringbuf_init(RX_BUFF_SIZE);
		rx_state.pkts[i].state = RX_STATE_EMPTY;
		//size, crc = initialized at start byte
		// data initialized first byte after start byte.
	}
	
	IRRX_USART_PORT.DIRCLR = _RXPIN_bm;

	/** These values are from G4CONFIG, NOT board config **/
	IRRX_USART.BAUDCTRLA = (uint8_t)(IR_USART_BSEL&0x00FF);
	IRRX_USART.BAUDCTRLB = (IR_USART_BSCALE<<USART_BSCALE_gp) | (uint8_t)( (IR_USART_BSEL>>8) & 0x0F );

	IRRX_USART.CTRLA |= USART_RXCINTLVL_MED_gc;
	IRRX_USART.CTRLB |= USART_RXEN_bm;

	//NOTE : removed 1<<USART_SBMODE_bp Why does /LW use 2 stop bits?
	IRRX_USART.CTRLC = USART_PMODE_DISABLED_gc | USART_CHSIZE_9BIT_gc ;
}

void ir_rx(ir_pkt_t * pkt) {
	//tells the caller whether data was returned: default to no
	pkt->size = 0;

	if ( rx_state.pkts[ rx_state.read ].state == RX_STATE_EMPTY )
		return;

	//@TODO max bytes to process in one run because we dont have preemptive multiprocessing!
	uint8_t i = 0;
	const uint8_t process_max = 10;
	while ( !ringbuf_empty(rx_state.pkts[ rx_state.read ].buf) && rx_state.pkts[ rx_state.read ].state == RX_STATE_RECEIVE  && i++ < process_max ) 
		process_rx_byte();

	if ( rx_state.pkts[ rx_state.read ].state == RX_STATE_PROCESS ) {
		
		//the packet's last byte should always be a CRC of the whole packet.
		if ( rx_state.pkts[ rx_state.read ].crc == rx_state.pkts[ rx_state.read ].data[ rx_state.pkts[rx_state.read].size - 1] ) {
			pkt->data = rx_state.pkts[ rx_state.read ].data;
			pkt->size = rx_state.pkts[ rx_state.read ].size;

			//really not entirely necessary...
			realloc(pkt->data, pkt->size);
		} else {
			free(rx_state.pkts[ rx_state.read ].data);
		}

		//while the state is RX_STATE_PROCESS, the ISR will not write any data to this buffer. 
		ringbuf_flush( rx_state.pkts[ rx_state.read ].buf );
		rx_state.pkts[ rx_state.read ].state = RX_STATE_EMPTY;
	
		//now the question is: increment read or leave it?
		if ( rx_state.read != rx_state.write )
			rx_state.read = (rx_state.read >= N_BUFF) ? 0 : rx_state.read+1;

	}
}

static inline void process_rx_byte(void) {
	uint8_t data = ringbuf_get(rx_state.pkts[rx_state.read].buf);

	if ( rx_state.pkts[rx_state.read].size == 0 ) {

//		rx_state.pkts[rx_state.read].state = RX_STATE_RECEIVE;
		//until I figure out how to know how many bytes are in the paket based on the command...
		rx_state.pkts[rx_state.read].data = (uint8_t*)malloc(MAX_PKT_SIZE);
		rx_state.pkts[rx_state.read].size = MAX_PKT_SIZE;

		rx_state.pkts[rx_state.read].data[0] = data;
		rx_state.pkts[rx_state.read].size = 1;
	} else {

		//previous byte was the header CRC.
		if ( rx_state.pkts[rx_state.read].size == offsetof(ir_hdr_t,crc)+1 ) {
			//in the case where the CRC is correct, this is going to get checked twice (also by ir_rx,
			//but that also tests the final CRC of "long" packets.)
			if ( rx_state.pkts[rx_state.read].crc != rx_state.pkts[rx_state.read].data[rx_state.pkts[rx_state.read].size -1] ) {
				//this is a bit of a hack to jump to the cleanup in ir_rx().
				//by setting RX_STATE_PROCESS, we guarantee that the ISR will not put more data into the ringbuf
				rx_state.pkts[rx_state.read].state = RX_STATE_PROCESS;

				return;
			}
		} else {
			crc(&rx_state.pkts[rx_state.read].crc, rx_state.pkts[rx_state.read].size-1, IR_CRC_POLY);
		}
		rx_state.pkts[rx_state.read].data[rx_state.pkts[rx_state.read].size] = data;
				
		if ( ++rx_state.pkts[rx_state.read].size == rx_state.pkts[rx_state.read].max_size )
			rx_state.pkts[rx_state.read].state = RX_STATE_PROCESS;
	}
}

/**
 * Triggers an RX timeout and forces received data in a buffer to be processed.
 */
static void rx_timer_tick(void) {

	//only need to time out reception if it is receiving
	if ( rx_state.pkts[rx_state.read].state != RX_STATE_RECEIVE )
		return;

	if ( ++rx_state.pkts[rx_state.read].timer == RX_TIMEOUT_TICKS )
		rx_state.pkts[rx_state.read].state = RX_STATE_PROCESS;
}


ISR(IRRX_USART_RXC_vect) {
	uint8_t bit8 = IRRX_USART.STATUS & USART_RXB8_bm;
	uint8_t data = IRRX_USART.DATA;

	/**
	 * This represents the start of a new packet
	 */
	if ( (data == 0xFF && !bit8) && rx_state.pkts[ rx_state.read ].state != RX_STATE_READY ) {
		
		uint8_t idx = rx_state.write;
		
		if ( rx_state.pkts[ idx ].state != RX_STATE_EMPTY ) {
			//buffer will receive no new bytes -> process remaining bytes then set to empty.
			rx_state.pkts[ idx ].state = RX_STATE_PROCESS;

			if ( ++idx >= N_BUFF ) 
				idx = 0;
			
			if ( rx_state.pkts[ idx ].state != RX_STATE_EMPTY )  
				free( rx_state.pkts[ idx ].data );

			rx_state.write = idx;
		}

		rx_state.pkts[idx].state = RX_STATE_READY;
		rx_state.pkts[idx].crc = IR_CRC_SHIFT;
		rx_state.pkts[idx].size = 0;
		ringbuf_flush(rx_state.pkts[idx].buf);

	//so much for keeping processing out of the ISR?...
	
	//received a valid data byte, and at some point before said byte... there was a start byte... 
	/**
	 * @TODO when state is set to ready, start a timer that... ticks. Timer is reset when bytes are 
	 * received. if timer > N, then set the state to process or empty.
	 */
	} else if ( rx_state.pkts[ rx_state.write ].state == RX_STATE_RECEIVE ) {
		rx_state.pkts[ rx_state.write ].timer = 0;
		ringbuf_put( rx_state.pkts[ rx_state.write ].buf , data );
	} else if ( rx_state.pkts[ rx_state.write ].state == RX_STATE_READY ) {
		rx_state.pkts[ rx_state.write ].state = RX_STATE_RECEIVE;
		rx_state.pkts[ rx_state.write ].timer = 0;
		ringbuf_put( rx_state.pkts[ rx_state.write ].buf, data );
	} else { 
		///fuck off
		//this could happen if the receiver sets the state to RX_STATE_PROCESS, meaning that the 
		//packet is "complete" and should not receive any more bytes.
	}
}
