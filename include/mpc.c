#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <avr/io.h>
#include <avr/interrupt.h>

#include <g4config.h>
#include "config.h"
#include <mpc.h>
#include <ringbuf.h>
#include <util.h>

#ifndef MPC_QUEUE_SIZE
	#define MPC_QUEUE_SIZE 8
#endif

/**
 * "template" for recvq and sendq so the inc/decrement logic can be shared
 */
typedef struct {
	uint8_t read;
	uint8_t write;
} qhdr_t;


/**
 * Receiver state
 */
static struct {
	uint8_t size;
	uint8_t crc;
	ringbuf_t * buf;
	pkt_hdr * pkt;

	struct {
		qhdr_t hdr;
		pkt_hdr * items[MPC_QUEUE_SIZE];
	} queue;

} rx_state ;

/** 
 * keep track of the state of teh sender.
 */

static struct {

	uint8_t byte; 
	uint8_t len;
	uint8_t * data;

	enum {
		TX_STATUS_IDLE = 0,
		TX_STATUS_BUSY = 1
	} status;

	struct {
		qhdr_t hdr;

		struct { 
			uint8_t addr;
			uint8_t len;
			uint8_t * data;
		} items[MPC_QUEUE_SIZE];

	} queue;

} tx_state;

static void mpc_slave_init(void);
static void mpc_master_init(void);

static bool queue_empty(qhdr_t);
static void queue_rd_idx(qhdr_t q);
static void queue_wr_idx(qhdr_t q);

static void mpc_slave_recv_byte(uint8_t);
static void mpc_slave_recv_pkt(pkt_hdr * pkt);

static void mpc_master_end_txn(void);
static void mpc_master_run(void);

/** 
 * @TODo smart mode?
 */


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

inline void mpc_init() {

	 #ifdef MPC_TWI_ADDR_EEPROM_ADDR
	mpc_twi_addr = eeprom_read_byte((uint8_t*)MPC_TWI_ADDR_EEPROM_ADDR)<<1;
	#endif

	mpc_master_init();
	mpc_slave_init();
}


static inline void mpc_master_init() {

	MPC_TWI.MASTER.CTRLA |= TWI_MASTER_INTLVL_MED_gc | TWI_MASTER_ENABLE_bm | TWI_MASTER_WIEN_bm;
	MPC_TWI.MASTER.BAUD = MPC_TWI_BAUD;

	//per AVR1308
	MPC_TWI.MASTER.STATUS = TWI_MASTER_BUSSTATE_IDLE_gc;
	
	memset(&tx_state,0 , sizeof tx_state);
	/** @TODO set queue read/write and status separately here*/
}

static inline void mpc_slave_init() {
	

	/**@Todo better initialization here*/
	memset(&rx_state, 0, sizeof rx_state);
	rx_state.buf = ringbuf_init(MPC_QUEUE_SIZE);


	MPC_TWI.SLAVE.CTRLA = TWI_SLAVE_INTLVL_LO_gc | TWI_SLAVE_ENABLE_bm | TWI_SLAVE_APIEN_bm /*| TWI_SLAVE_PMEN_bm*/;

	MPC_TWI.SLAVE.ADDR = mpc_twi_addr;

#ifdef MPC_TWI_ADDRMASK
	MPC_TWI.SLAVE.ADDRMASK = MPC_TWI_ADDRMASK << 1;
#endif
}

static inline bool queue_empty(qhdr_t q) {
	return q.read == q.write;
}

static inline void queue_rd_idx(qhdr_t q) {
	q.read = (q.read == MPC_QUEUE_SIZE-1) ? 0 : q.read+1;
}

static inline void queue_wr_idx(qhdr_t q) {
	q.write = (q.read == MPC_QUEUE_SIZE-1) ? 0 : q.write+1;
}

/**
 * Process queued bytes into packtes.
 */

inline pkt_hdr* mpc_recv() {

	mpc_master_run();

	while ( !ringbuf_empty(rx_state.buf) )
		mpc_slave_recv_byte(ringbuf_get(rx_state.buf));

	if ( queue_empty(rx_state.queue.hdr) ) return NULL;

	pkt_hdr * pkt = rx_state.queue.items[ rx_state.queue.hdr.read ];
	queue_rd_idx(rx_state.queue.hdr);

	return pkt;
}



static inline void mpc_slave_recv_byte(uint8_t data) {

	//@TODO: these hardcoded byte offsets are a mess 
	//(I write this as I'm changing them to modify packet structure...)
	if ( rx_state.size == 0 ) {
		rx_state.pkt = (pkt_hdr*)malloc(sizeof(pkt_hdr));
		rx_state.crc = MPC_CRC_SHIFT;
		rx_state.pkt->cmd = data;
		crc(&rx_state.crc, data, MPC_CRC_POLY);
		rx_state.size = 1;

	} else if ( rx_state.size == 1 ) {
		rx_state.pkt->len = data;
		crc(&rx_state.crc, data, MPC_CRC_POLY);
		rx_state.size = 2;

	} else if ( rx_state.size == 2 ) {
		rx_state.pkt->saddr = data;
		crc(&rx_state.crc, data, MPC_CRC_POLY);
		rx_state.size = 3;

		if ( rx_state.pkt->len > 0 )
			rx_state.pkt->data = (uint8_t *)malloc(rx_state.pkt->len);

	//case 1: there is no payload, so byte 3 is the CRC
	//case 2: there is a payload, but we're past it, so this is the CRC.
	} else if ( (rx_state.size == 3 && rx_state.pkt->len == 0) || (rx_state.pkt->len > 0 && rx_state.size >= rx_state.pkt->len+3) ) {
		rx_state.pkt->chksum = data;
	
	//receive the payload.
	} else if ( rx_state.size >= 3 && rx_state.size < rx_state.pkt->len+3 ) {
		rx_state.pkt->data[rx_state.size-3] = data;
		crc(&rx_state.crc, data, MPC_CRC_POLY);
		rx_state.size++;
	} else {
	//uH???	
	//	recv.size = 0;
	}
}



static inline void mpc_slave_recv_pkt(pkt_hdr * pkt) {

	//before returning the packet, flush the bufffarrrr
	while ( !ringbuf_empty(rx_state.buf) )
		mpc_slave_recv_byte(ringbuf_get(rx_state.buf));

	rx_state.queue.items[ rx_state.queue.hdr.write ] = pkt;
	queue_wr_idx(rx_state.queue.hdr);
}

MPC_TWI_SLAVE_ISR {

    // If address match. 
	if ( (MPC_TWI.SLAVE.STATUS & TWI_SLAVE_APIF_bm) &&  (MPC_TWI.SLAVE.STATUS & TWI_SLAVE_AP_bm) ) {

#ifdef MPC_TWI_ADDRMASK
		uint8_t addr = MPC_TWI.SLAVE.DATA;
		if ( addr & mpc_twi_addr ) {
#endif

			if ( rx_state.pkt != NULL ) {
				if ( rx_state.pkt->data != NULL )
					free(rx_state.pkt->data);
				free(rx_state.pkt);
				rx_state.pkt = NULL;
			}

			//set data interrupt because we actually give a shit
			MPC_TWI.SLAVE.CTRLA |= TWI_SLAVE_DIEN_bm;

			//also a stop interrupt.. Wait that's enabled with APIEN?
			MPC_TWI.SLAVE.CTRLA |= TWI_SLAVE_PIEN_bm;

			//this is SUPER temp //OR IS IT??? HMMM HMM YEAH ID IDDDDDDD MHHHM
			rx_state.size = 0;
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
			ringbuf_put(rx_state.buf, MPC_TWI.SLAVE.DATA);
			//@TODO SMART MODE
			MPC_TWI.SLAVE.CTRLB = TWI_SLAVE_CMD_RESPONSE_gc;
		}
	} else if ( MPC_TWI.SLAVE.STATUS & TWI_SLAVE_APIF_bm ) {
		//for now, we'll just change this to copy the data to a new packet
		//but later, we should just keep everything in the queue, separate the recv state
		//@TODO ^^

		if ( rx_state.crc == rx_state.pkt->chksum )
			mpc_slave_recv_pkt(rx_state.pkt);
		else {
			if ( rx_state.pkt->len > 0 )
				free(rx_state.pkt->data);
			free(rx_state.pkt);
		}

		rx_state.pkt = NULL;

	    /* Disable stop interrupt. */
    	MPC_TWI.SLAVE.CTRLA &= ~TWI_SLAVE_PIEN_bm;

    	MPC_TWI.SLAVE.STATUS |= TWI_SLAVE_APIF_bm;

		MPC_TWI.SLAVE.CTRLA &= ~TWI_SLAVE_DIEN_bm;
	}

	// If unexpected state.
	else {
		if ( rx_state.pkt != NULL ) {
			if ( rx_state.pkt->data != NULL )
				free(rx_state.pkt->data);
			free(rx_state.pkt);
			rx_state.pkt = NULL;
		}
		//todo?
		//TWI_SlaveTransactionFinished(twi, TWIS_RESULT_FAIL);
	}
};


/****
 * END TWI SLAVE CODE - BEGIN MASTER
 * (why did I combine the filez?)
 */

inline void mpc_send_cmd(const uint8_t addr, const uint8_t cmd) {
	mpc_send(addr, cmd, NULL, 0);
}

//CALLER MUST FREE() data*
void mpc_send(const uint8_t addr, const uint8_t cmd, uint8_t * const data, const uint8_t len) {
	/**
	 * Rather than actually use the "pkt_hdr" struct, just use the same structure 
	 * (Allows me to just malloc a contiguous chunk of data to send to the transmitter)
	 */
	const uint8_t pkt_hdr_size = 3; 
	const uint8_t pkt_tail_size = 1; //i.e. CRC 
	uint8_t * const raw = (uint8_t*)malloc(len+pkt_hdr_size+pkt_tail_size) ;
	uint8_t chksum = MPC_CRC_SHIFT;

	raw[0] = cmd;
	raw[1] = len;
	raw[2] = mpc_twi_addr;

	crc(&chksum, cmd, MPC_CRC_POLY);
	crc(&chksum, len, MPC_CRC_POLY);
	crc(&chksum, mpc_twi_addr, MPC_CRC_POLY);

	for ( uint8_t i = 0; i < len; ++i ) {
		raw[i+pkt_hdr_size] = data[i];
		crc(&chksum, data[i], MPC_CRC_POLY);
	}

	raw[len+pkt_hdr_size] = chksum;
	
	tx_state.queue.items[ tx_state.queue.hdr.write ].addr = addr; 
	tx_state.queue.items[ tx_state.queue.hdr.write ].len = len+pkt_hdr_size+pkt_tail_size; //data+header
	tx_state.queue.items[ tx_state.queue.hdr.write ].data = raw;
	queue_wr_idx(tx_state.queue.hdr);
}

static void mpc_master_run() {

	//notthing to do harr.
	if ( queue_empty(tx_state.queue.hdr) || tx_state.status != TX_STATUS_IDLE ) return; 

	tx_state.status = TX_STATUS_BUSY;
		
	tx_state.data = tx_state.queue.items[ tx_state.queue.hdr.read ].data;
	tx_state.len = tx_state.queue.items[ tx_state.queue.hdr.read ].len;
	tx_state.byte = 0;

	MPC_TWI.MASTER.ADDR = tx_state.queue.items[ tx_state.queue.hdr.read ].addr<<1 | 0;
	queue_rd_idx(tx_state.queue.hdr);
}

static inline void mpc_master_end_txn(void) {
	if ( tx_state.len > 0 )
		free(tx_state.data);
	tx_state.status = TX_STATUS_IDLE;
}

MPC_TWI_MASTER_ISR  {

    if ( (MPC_TWI.MASTER.STATUS & TWI_MASTER_ARBLOST_bm) || (MPC_TWI.MASTER.STATUS & TWI_MASTER_BUSERR_bm)) {
		/** Go back to the first byte and try the transaction over. 
		 * @TODO: What is a BUSERROR? And what happens if the slave receives part data? CRC would help here. 
		 **/
		tx_state.byte = 0;
		tx_state.status = TX_STATUS_IDLE;

		//per AVR1308 example code.
		MPC_TWI.MASTER.STATUS |= TWI_MASTER_ARBLOST_bm;
	} else if ( MPC_TWI.MASTER.STATUS & TWI_MASTER_WIF_bm ) {

		//@TODO: need to handle this better! at the very least, end the txn?	
		//when it is read as 0, most recent ack bit was NAK. 
		if (MPC_TWI.MASTER.STATUS & TWI_MASTER_RXACK_bm) {
			//WHY IS THIS HERE???
			MPC_TWI.MASTER.CTRLC = TWI_MASTER_CMD_STOP_gc;
			MPC_TWI.MASTER.STATUS = TWI_MASTER_BUSSTATE_IDLE_gc;
			tx_state.byte = 0;
			tx_state.status = TX_STATUS_IDLE;

		} else {
			if ( tx_state.byte < tx_state.len ) {
				MPC_TWI.MASTER.DATA  = tx_state.data[tx_state.byte++];
			} else {
				MPC_TWI.MASTER.CTRLC = TWI_MASTER_CMD_STOP_gc;
				MPC_TWI.MASTER.STATUS = TWI_MASTER_BUSSTATE_IDLE_gc;
				mpc_master_end_txn();
			}

		}
	//IDFK??
	} else {
		MPC_TWI.MASTER.CTRLC = TWI_MASTER_CMD_STOP_gc;
		
		//not sure wtf this is, so unlike the other errors, we'll just flush the dataaaa
		mpc_master_end_txn();
	}
}
