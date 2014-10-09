/**
 * gcc --std=c99 -I ../include ../include/util.c ../include/ringbuf.c  test.c -o test -O0 -g -v  -da -QA
 */

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

//#include <avr/io.h>
//#include <avr/interrupt.h>

#include <util.h>
//#include "config.h"
#include "g4config.h"
//#include <mpc.h>
#include <irrx.h>
//#include <leds.h>
#include <ringbuf.h>
//#include "timer.h"

#define _RXPIN_bm G4_PIN(IRRX_USART_RXPIN)

#define N_BUFF 4
#define RX_BUFF_SIZE 15
#define MAX_PKT_SIZE 15

//when the packet timer reaches this level, set the state to process.
// 1/IR_BAUD*mil = uS per bit. Timeout after 11 bits. (too generous)
//this is the number of timer ticks per IR bits.
//#define RX_TIMEOUT_TIME (task_freq_t)(1/IR_BAUD*1000000/TIMER_TICK_US)
//#define RX_TIMEOUT_TICKS 11 //time out after this many bit widths with no data (too low?)

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
			RX_STATE_EMPTY = 0,
			//ready to receive data
			RX_STATE_READY = 1,
			//receiving data 
			RX_STATE_RECEIVE = 2,
			//no more data will be received for this pkt,
			//so finish processing remaining bytes then return pkt.
			RX_STATE_PROCESS = 3,
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

	uint8_t scheduled;
} rx_state;


static inline void process_rx_byte(void);
static void rx_timer_tick(void);
void ISR_RECEIVE_BYTE(uint8_t, uint8_t);
void fml_free(void * foo) {
//	printf("free: 0x%016x\n", foo);
	free(foo);
	foo = NULL;
}

int main(void) {

	irrx_init();

	ir_pkt_t pkt;


	int data[] = { 
		255, 56, 127 ,138,103,83,0,15,15,68,72,0,44,1,88,113, 1,2,3,4,5,255,6,6,7,8,0,  255,255,255,67, 255, 56, 127 ,138,103,83,0,15,15,68,72,0,44,1,88,113, 
		255, 56, 127 ,138,103,83,0,15,15,68,72,0,44,1,88,113, 1,2,3,4,5,255,6,6,7,8,0,  255,255,255,67, 255, 56, 127 ,138,103,83,0,15,15,68,72,0,44,1,88,113, 
		255, 56, 127 ,138,103,83,0,15,15,68,72,0,44,1,88,113, 1,2,3,4,5,255,6,6,7,8,0,  255,255,255,67, 255, 56, 127 ,138,103,83,0,15,15,68,72,0,44,1,88,113, 
		255, 56, 127 ,138,103,83,0,15,15,68,72,0,44,1,88,113, 1,2,3,4,5,255,6,6,7,8,0,  255,255,255,67, 255, 56, 127 ,138,103,83,0,15,15,68,72,0,44,1,88,113, 
		255, 56, 127 ,138,103,83,0,15,15,68,72,0,44,1,88,113, 1,2,3,4,5,255,6,6,7,8,0,  255,255,255,67, 255, 56, 127 ,138,103,83,0,15,15,68,72,0,44,1,88,113, 
		255, 56, 127 ,138,103,83,0,15,15,68,72,0,44,1,88,113, 1,2,3,4,5,255,6,6,7,8,0,  255,255,255,67, 255, 56, 127 ,138,103,83,0,15,15,68,72,0,44,1,88,113, 
		255, 56, 127 ,138,103,83,0,15,15,68,72,0,44,1,88,113, 1,2,3,4,5,255,6,6,7,8,0,  255,255,255,67, 255, 56, 127 ,138,103,83,0,15,15,68,72,0,44,1,88,113, 
		255, 56, 127 ,138,103,83,0,15,15,68,72,0,44,1,88,113, 1,2,3,4,5,255,6,6,7,8,0,  255,255,255,67, 255, 56, 127 ,138,103,83,0,15,15,68,72,0,44,1,88,113, 
		255, 56, 127 ,138,103,83,0,15,15,68,72,0,44,1,88,113, 1,2,3,4,5,255,6,6,7,8,0,  255,255,255,67, 255, 56, 127 ,138,103,83,0,15,15,68,72,0,44,1,88,113, 
		255, 56, 127 ,138,103,83,0,15,15,68,72,0,44,1,88,113, 1,2,3,4,5,255,6,6,7,8,0,  255,255,255,67, 255, 56, 127 ,138,103,83,0,15,15,68,72,0,44,1,88,113, 
		255, 56, 127 ,138,103,83,0,15,15,68,72,0,44,1,88,113, 1,2,3,4,5,255,6,6,7,8,0,  255,255,255,67, 255, 56, 127 ,138,103,83,0,15,15,68,72,0,44,1,88,113, 
		255, 56, 127 ,138,103,83,0,15,15,68,72,0,44,1,88,113, 1,2,3,4,5,255,6,6,7,8,0,  255,255,255,67, 255, 56, 127 ,138,103,83,0,15,15,68,72,0,44,1,88,113, 
		255, 56, 127 ,138,103,83,0,15,15,68,72,0,44,1,88,113, 1,2,3,4,5,255,6,6,7,8,0,  255,255,255,67, 255, 56, 127 ,138,103,83,0,15,15,68,72,0,44,1,88,113, 
		255, 56, 127 ,138,103,83,0,15,15,68,72,0,44,1,88,113, 1,2,3,4,5,255,6,6,7,8,0,  255,255,255,67, 255, 56, 127 ,138,103,83,0,15,15,68,72,0,44,1,88,113, 
		255, 56, 127 ,138,103,83,0,15,15,68,72,0,44,1,88,113, 1,2,3,4,5,255,6,6,7,8,0,  255,255,255,67, 255, 56, 127 ,138,103,83,0,15,15,68,72,0,44,1,88,113, 

		-1,-1,-1,-1
	};
	
	uint8_t meh[] = {2,2};
	uint8_t gogo = 1;
	int rx_cnt = 0;
	int proc_cnt = 0;
	int i = 0;

	while (1) {

		for (int j = 0; j < 2; ++j ) {
			uint8_t byte = (uint8_t)(data[i++]&0xff);
			uint8_t bit8 = (byte != 255);

			if ( data[i] >= 0 ) {
				++rx_cnt;
				ISR_RECEIVE_BYTE(bit8,byte);
			} else {
				gogo = 0;
				break;
			}
		}

		do {
			ir_rx(&pkt) ;
			proc_cnt++;
//			printf("%d/%d\n",proc_cnt,rx_cnt);
			if (pkt.size > 0)
			printf("%02d: rx a packet, size = %d\n",i, pkt.size);
			if ( pkt.size != 0) {
				free(pkt.data);
				pkt.data = NULL;pkt.size = 0;
			}
		} while (gogo==0 && proc_cnt <= rx_cnt);

		if ( gogo == 0 ) break;
/////
	}
	return 0;
}

inline void irrx_init(void) {

	rx_state.read = 0;
	rx_state.write = 0;
	rx_state.scheduled = 0; //hmm?

	for (uint8_t i = 0; i < N_BUFF; ++i) {
		rx_state.pkts[i].buf = ringbuf_init(RX_BUFF_SIZE);
		rx_state.pkts[i].state = RX_STATE_EMPTY;
		//size, crc = initialized at start byte
		// data initialized first byte after start byte.
	}
	
//	IRRX_USART_PORT.DIRCLR = _RXPIN_bm;

	/** These values are from G4CONFIG, NOT board config **/
//	IRRX_USART.BAUDCTRLA = (uint8_t)(IR_USART_BSEL&0x00FF);
//	IRRX_USART.BAUDCTRLB = (IR_USART_BSCALE<<USART_BSCALE_gp) | (uint8_t)( (IR_USART_BSEL>>8) & 0x0F );

//	IRRX_USART.CTRLA |= USART_RXCINTLVL_MED_gc;
///	IRRX_USART.CTRLB |= USART_RXEN_bm;

	//NOTE : removed 1<<USART_SBMODE_bp Why does /LW use 2 stop bits?
//	IRRX_USART.CTRLC = USART_PMODE_DISABLED_gc | USART_CHSIZE_9BIT_gc ;


}

void ir_rx(ir_pkt_t * pkt) {
	//tells the caller whether data was returned: default to no
	pkt->size = 0;

	if ( rx_state.pkts[ rx_state.read ].state == RX_STATE_EMPTY )
		return;

	//printf("Queue is non empty (%d)! %d/%d", rx_state.pkts[ rx_state.read ].state, rx_state.read, rx_state.write);
	//@TODO max bytes to process in one run because we dont have preemptive multiprocessing!
	uint8_t i = 0;
	const uint8_t process_max = 10;	
	while ( !ringbuf_empty(rx_state.pkts[ rx_state.read ].buf) && i++ < process_max ) 
		process_rx_byte();

	
	if ( rx_state.pkts[ rx_state.read ].state == RX_STATE_PROCESS ) {

		//the packet's last byte should always be a CRC of the whole packet.
		if ( rx_state.pkts[ rx_state.read ].crc == rx_state.pkts[ rx_state.read ].data[ rx_state.pkts[rx_state.read].size - 1] ) {

	//		printf("Processing a packet. %d/%d\n", rx_state.read, rx_state.write);

			pkt->data = rx_state.pkts[ rx_state.read ].data;// realloc( rx_state.pkts[ rx_state.read ].data, pkt->size );
			pkt->size = rx_state.pkts[ rx_state.read ].size;

			//temp
			static uint8_t on = 0;

//			if ( pkt->data[0] == 0x38 )
//				set_lights(on^=1);


		} else {
	//		printf("Trashed due to CRC\n");
			fml_free(rx_state.pkts[ rx_state.read ].data);
		}
		
		rx_state.pkts[rx_state.read].data = NULL;

		//while the state is RX_STATE_PROCESS, the ISR will not write any data to this buffer. 
		ringbuf_flush( rx_state.pkts[ rx_state.read ].buf );
		rx_state.pkts[ rx_state.read ].state = RX_STATE_EMPTY;
	

//		sei(); 
		//now the question is: increment read or leave it?
		if ( rx_state.read != rx_state.write ) {
	//		printf("*****Read and write are different.\n");
			rx_state.read = (rx_state.read == N_BUFF-1) ? 0 : rx_state.read+1;
		}
		else {
//			printf("*****Read and write are at the same position\n");
//			timer_unregister(rx_timer_tick);
//			rx_state.scheduled = 0;
		}

		//		cli();

	} else {
//		printf("QUEUE %d is not processed.\n", rx_state.read);
	}
}

static inline void process_rx_byte(void) {
//	printf("Process byte\n");
	uint8_t data = ringbuf_get(rx_state.pkts[rx_state.read].buf);

	if ( rx_state.pkts[rx_state.read].size == 0 ) {

		uint8_t max_size = (data == 0x38) ? 15 : 4;
//		printf("Alloc: %d\n", max_size);
		rx_state.pkts[rx_state.read].data = (uint8_t*)malloc(max_size);
		rx_state.pkts[rx_state.read].max_size = max_size;
		rx_state.pkts[rx_state.read].crc = IR_CRC_SHIFT;

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

				
		//		printf("Expected a CRC of 0x%02x, but have 0x%02x\n", rx_state.pkts[rx_state.read].data[rx_state.pkts[rx_state.read].size -1], rx_state.pkts[rx_state.read].crc);
				return;
			}
		} else {
//			printf("CRC: 0x%02x\n", rx_state.pkts[rx_state.read].data[rx_state.pkts[rx_state.read].size-1] );
			crc(&rx_state.pkts[rx_state.read].crc, rx_state.pkts[rx_state.read].data[rx_state.pkts[rx_state.read].size-1], IR_CRC_POLY);
		}
		rx_state.pkts[rx_state.read].data[rx_state.pkts[rx_state.read].size] = data;
				
		if ( ++rx_state.pkts[rx_state.read].size == rx_state.pkts[rx_state.read].max_size ) {
			rx_state.pkts[rx_state.read].state = RX_STATE_PROCESS;
		//	printf("Process due to max size! %d/%d", rx_state.pkts[rx_state.read].size, rx_state.pkts[rx_state.read].max_size);
		}
	}
}

/**
 * Triggers an RX timeout and forces received data in a buffer to be processed.
 */
/*&static void rx_timer_tick(void) {

	//only need to time out reception if it is receiving
	if ( rx_state.pkts[rx_state.read].state != RX_STATE_RECEIVE )
		return;

	if ( ++rx_state.pkts[rx_state.read].timer == RX_TIMEOUT_TICKS )
		rx_state.pkts[rx_state.read].state = RX_STATE_PROCESS;
}*/


void ISR_RECEIVE_BYTE(uint8_t bit8, uint8_t data) {

	/**
	 * This represents the start of a new packet
	 */
	if ( (data == 0xFF && !bit8) && rx_state.pkts[ rx_state.read ].state != RX_STATE_READY ) {
		printf("Received a start byte with a dirty queue.\n");
		uint8_t idx = rx_state.write;
		
		if ( rx_state.pkts[ idx ].state != RX_STATE_EMPTY ) {
			//buffer will receive no new bytes -> process remaining bytes then set to empty.
			rx_state.pkts[ idx ].state = RX_STATE_PROCESS;
			printf("process due to rotate (size: %d; wr: %d; rd:%d)\n", rx_state.pkts[rx_state.write].size,rx_state.write,rx_state.read);
			if ( ++idx == N_BUFF ) 
				idx = 0;
			
			if ( rx_state.pkts[ idx ].state != RX_STATE_EMPTY )  {
//				printf("2\n");
				fml_free( rx_state.pkts[ idx ].data );
				rx_state.pkts[idx].data = NULL;
			}

			rx_state.write = idx;
			
//			if ( !rx_state.scheduled ) {
//				rx_state.scheduled = 1;
//				timer_register(rx_timer_tick, RX_TIMEOUT_TIME , TIMER_RUN_UNLIMITED);
//			}
		}

		rx_state.pkts[idx].state = RX_STATE_READY;
		rx_state.pkts[idx].size = 0;
		ringbuf_flush(rx_state.pkts[idx].buf);

	//so much for keeping processing out of the ISR?...
	
	//received a valid data byte, and at some point before said byte... there was a start byte... 
	/**
	 * @TODO when state is set to ready, start a timer that... ticks. Timer is reset when bytes are 
	 * received. if timer > N, then set the state to process or empty.
	 */
	} else if ( rx_state.pkts[ rx_state.write ].state == RX_STATE_RECEIVE ) {
//		printf("RX: 0x%02x\n", data);
		rx_state.pkts[ rx_state.write ].timer = 0;
		ringbuf_put( rx_state.pkts[ rx_state.write ].buf , data );
	} else if ( rx_state.pkts[ rx_state.write ].state == RX_STATE_READY ) {
//		printf("*RX 0x%02x\n", data);

		rx_state.pkts[ rx_state.write ].state = RX_STATE_RECEIVE;
		rx_state.pkts[ rx_state.write ].timer = 0;
		ringbuf_put( rx_state.pkts[ rx_state.write ].buf, data );
	} else {
		///fuck off
		//this could happen if the receiver sets the state to RX_STATE_PROCESS, meaning that the 
		//packet is "complete" and should not receive any more bytes.
	}
}
