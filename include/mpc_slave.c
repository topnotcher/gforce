#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <avr/io.h>
#include <avr/interrupt.h>

#include "g4config.h"
#include "config.h"

#include <ringbuf.h>
#include <util.h>
#include <pkt.h>
//#include <ringbuf.h>
#include <mpc_slave.h>

static void mpc_recv_byte(uint8_t);
static void init_recv(void);
static void recv_pkt(pkt_hdr * pkt);

typedef struct {
	uint8_t size;
	uint8_t crc;
	pkt_hdr * pkt;
} recv_state;


recv_state recv;
ringbuf_t * recvbuf; 

/**
 * Queue of received packets to be processed
 */
#define MPC_QUEUE_SIZE 8
typedef struct {
	uint8_t read;
	uint8_t write;
	pkt_hdr * items[MPC_QUEUE_SIZE];
} mpc_queue_t;

mpc_queue_t recvq;


/**
 * The shoulders need to differentiate left/right.
 */
#ifdef MPC_TWI_ADDR_EEPROM_ADDR
#include <avr/eeprom.h>
static uint8_t mpc_twi_addr ;
#endif

/** 
 * Otherwise, make it a constant that should optimize away most of the time.
 */
#ifdef MPC_TWI_ADDR
static const uint8_t mpc_twi_addr = (MPC_TWI_ADDR<<1);
#endif

/**
 * Initialize the MPC interface.
 */
inline void mpc_slave_init() {

	recvbuf = ringbuf_init(MPC_QUEUE_SIZE);
	init_recv();

	MPC_TWI.SLAVE.CTRLA = TWI_SLAVE_INTLVL_LO_gc | TWI_SLAVE_ENABLE_bm | TWI_SLAVE_APIEN_bm /*| TWI_SLAVE_PMEN_bm*/;

#ifdef MPC_TWI_ADDR_EEPROM_ADDR
	mpc_twi_addr = eeprom_read_byte((uint8_t*)MPC_TWI_ADDR_EEPROM_ADDR)<<1;
#endif
	MPC_TWI.SLAVE.ADDR = mpc_twi_addr;

#ifdef MPC_TWI_ADDRMASK
	MPC_TWI.SLAVE.ADDRMASK = MPC_TWI_ADDRMASK << 1;
#endif

	recvq.read = 0;
	recvq.write = 0;
}

static inline void init_recv() {
	recv.size = 0;
	recv.pkt = NULL;
	recv.crc = MPC_CRC_SHIFT;
}

/**
 * Process queued bytes into packtes.
 */

inline pkt_hdr* mpc_slave_recv() {

	while ( !ringbuf_empty(recvbuf) )
		mpc_recv_byte(ringbuf_get(recvbuf));

	if ( recvq.read == recvq.write) return NULL; 

	pkt_hdr * pkt = recvq.items[recvq.read];

	recvq.read = (recvq.read == MPC_QUEUE_SIZE-1) ? 0 : recvq.read+1;

	return pkt;
}

static inline void mpc_recv_byte(uint8_t data) {

	//@TODO: these hardcoded byte offsets are a mess 
	//(I write this as I'm changing them to modify packet structure...)
	if ( recv.size == 0 ) {
		recv.pkt = (pkt_hdr*)malloc(sizeof(pkt_hdr));
		recv.crc = MPC_CRC_SHIFT;
		recv.pkt->cmd = data;
		crc(&recv.crc, data, MPC_CRC_POLY);
		recv.size = 1;

	} else if ( recv.size == 1 ) {
		recv.pkt->len = data;
		crc(&recv.crc, data, MPC_CRC_POLY);
		recv.size = 2;

	} else if ( recv.size == 2 ) {
		recv.pkt->saddr = data;
		crc(&recv.crc, data, MPC_CRC_POLY);
		recv.size = 3;

		if ( recv.pkt->len > 0 )
			recv.pkt->data = (uint8_t *)malloc(recv.pkt->len);

	//case 1: there is no payload, so byte 3 is the CRC
	//case 2: there is a payload, but we're past it, so this is the CRC.
	} else if ( (recv.size == 3 && recv.pkt->len == 0) || (recv.pkt->len > 0 && recv.size >= recv.pkt->len+3) ) {
		recv.pkt->chksum = data;
	
//		if ( recv.pkt.chksum == recv.crc )
//			recv_pkt(recv.pkt);
		//note: the receiving end is required to free() the data otherwise!
//		else if ( recv.pkt.len > 0 ) 
//			free(recv.pkt.data);

//		recv.size = 0;

	//receive the payload.
	} else if ( recv.size >= 3 && recv.size < recv.pkt->len+3 ) {
		recv.pkt->data[recv.size-3] = data;
		crc(&recv.crc, data, MPC_CRC_POLY);
		recv.size++;
	} else {
	//uH???	
	//	recv.size = 0;
	}
}

static inline void recv_pkt(pkt_hdr * pkt) {

	//before returning the packet, flush the bufffarrrr
	while ( !ringbuf_empty(recvbuf) )
		mpc_recv_byte(ringbuf_get(recvbuf));

	recvq.items[recvq.write] = pkt;
	recvq.write = (recvq.write == MPC_QUEUE_SIZE-1) ? 0 : recvq.write+1;
}

MPC_TWI_SLAVE_ISR {

    // If address match. 
	if ( (MPC_TWI.SLAVE.STATUS & TWI_SLAVE_APIF_bm) &&  (MPC_TWI.SLAVE.STATUS & TWI_SLAVE_AP_bm) ) {

#ifdef MPC_TWI_ADDRMASK
		uint8_t addr = MPC_TWI.SLAVE.DATA;
		if ( addr & mpc_twi_addr ) {
#endif

			if ( recv.pkt != NULL ) {
				if ( recv.pkt->data != NULL )
					free(recv.pkt->data);
				free(recv.pkt);
				recv.pkt = NULL;
			}

			//set data interrupt because we actually give a shit
			MPC_TWI.SLAVE.CTRLA |= TWI_SLAVE_DIEN_bm;

			//also a stop interrupt.. Wait that's enabled with APIEN?
			MPC_TWI.SLAVE.CTRLA |= TWI_SLAVE_PIEN_bm;

			//this is SUPER temp //OR IS IT??? HMMM HMM YEAH ID IDDDDDDD MHHHM
			recv.size = 0;
			MPC_TWI.SLAVE.CTRLB = TWI_SLAVE_CMD_RESPONSE_gc;
#ifdef MPC_TWI_ADDRMASK
		} else {
			//is this right?
			MPC_TWI.SLAVE.CTRLB = TWI_SLAVE_CMD_COMPTRANS_gc;
		}
#endif

	// data interrupt 
	} else if (MPC_TWI.SLAVE.STATUS & TWI_SLAVE_DIF_bm) {
		// slave write 
		if (MPC_TWI.SLAVE.STATUS & TWI_SLAVE_DIR_bm) {

		//slave read(master write)
		} else {
			ringbuf_put(recvbuf, MPC_TWI.SLAVE.DATA);
			//mpc_recv_byte(MPC_TWI.SLAVE.DATA);
			MPC_TWI.SLAVE.CTRLB = TWI_SLAVE_CMD_RESPONSE_gc;
//			ringbuf_put(recvq, MPC_TWI.SLAVE.DATA);
		}
	} else if ( MPC_TWI.SLAVE.STATUS & TWI_SLAVE_APIF_bm ) {
		//for now, we'll just change this to copy the data to a new packet
		//but later, we should just keep everything in the queue, separate the recv state
		//@TODO ^^

		if ( recv.crc == recv.pkt->chksum )
			recv_pkt(recv.pkt);
		else {
			if ( recv.pkt->len > 0 )
				free(recv.pkt->data);
			free(recv.pkt);
		}

		recv.pkt = NULL;

	    /* Disable stop interrupt. */
    	MPC_TWI.SLAVE.CTRLA &= ~TWI_SLAVE_PIEN_bm;

    	MPC_TWI.SLAVE.STATUS |= TWI_SLAVE_APIF_bm;

		MPC_TWI.SLAVE.CTRLA &= ~TWI_SLAVE_DIEN_bm;
	}

	// If unexpected state.
	else {
		if ( recv.pkt != NULL ) {
			if ( recv.pkt->data != NULL )
				free(recv.pkt->data);
			free(recv.pkt);
			recv.pkt = NULL;
		}
		//todo?
		//TWI_SlaveTransactionFinished(twi, TWIS_RESULT_FAIL);
	}
}
