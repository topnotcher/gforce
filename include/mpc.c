#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/crc16.h>

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
	mpc_pkt * pkt;

	struct {
		qhdr_t hdr;
		mpc_pkt * items[MPC_QUEUE_SIZE];
	} queue;

} rx_state ;

/** 
 * keep track of the state of teh sender.
 */

static struct {

	uint8_t byte; 
	uint8_t len;
	mpc_pkt * pkt ;

	enum {
		TX_STATUS_IDLE = 0,
		TX_STATUS_BUSY = 1
	} status;

	struct {
		qhdr_t hdr;

		struct {
			uint8_t addr;
			mpc_pkt * pkt;
		} items[MPC_QUEUE_SIZE];

	} queue;

} tx_state;

static inline void mpc_slave_init(void) ATTR_ALWAYS_INLINE ;
static inline void mpc_master_init(void) ATTR_ALWAYS_INLINE ;

static inline bool queue_empty(qhdr_t*) ATTR_ALWAYS_INLINE;
static inline void queue_rd_idx(qhdr_t *q) ATTR_ALWAYS_INLINE;
static inline void queue_wr_idx(qhdr_t *q) ATTR_ALWAYS_INLINE;

static void mpc_slave_recv_byte(uint8_t);
static void mpc_slave_recv_pkt(mpc_pkt * pkt);

static void mpc_master_end_txn(void);
static inline void mpc_master_run(void) ATTR_ALWAYS_INLINE;


#define mpc_crc(crc,data) _crc_ibutton_update(crc,data)

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

inline void mpc_init(void) {

	 #ifdef MPC_TWI_ADDR_EEPROM_ADDR
	mpc_twi_addr = eeprom_read_byte((uint8_t*)MPC_TWI_ADDR_EEPROM_ADDR)<<1;
	#endif

	mpc_master_init();
	mpc_slave_init();
}


static inline void mpc_master_init(void) {

	MPC_TWI.MASTER.CTRLA |= TWI_MASTER_INTLVL_MED_gc | TWI_MASTER_ENABLE_bm | TWI_MASTER_WIEN_bm;
	MPC_TWI.MASTER.BAUD = MPC_TWI_BAUD;

	//per AVR1308
	MPC_TWI.MASTER.STATUS = TWI_MASTER_BUSSTATE_IDLE_gc;
	
	memset(&tx_state,0 , sizeof tx_state);

	tx_state.status = TX_STATUS_IDLE;


	/** @TODO set queue read/write and status separately here*/
}

static inline void mpc_slave_init(void) {
	

	/**@Todo better initialization here*/
	memset(&rx_state, 0, sizeof rx_state);
	rx_state.buf = ringbuf_init(MPC_QUEUE_SIZE);


	MPC_TWI.SLAVE.CTRLA = TWI_SLAVE_INTLVL_LO_gc | TWI_SLAVE_ENABLE_bm | TWI_SLAVE_APIEN_bm /*| TWI_SLAVE_PMEN_bm*/;

	MPC_TWI.SLAVE.ADDR = mpc_twi_addr;

#ifdef MPC_TWI_ADDRMASK
	MPC_TWI.SLAVE.ADDRMASK = MPC_TWI_ADDRMASK << 1;
#endif
}

static inline bool queue_empty(qhdr_t * q) {
	return q->read == q->write;
}

static inline void queue_rd_idx(qhdr_t * q) {
	q->read = (q->read == MPC_QUEUE_SIZE-1) ? 0 : q->read+1;
}

static inline void queue_wr_idx(qhdr_t * q) {
	q->write = (q->write == MPC_QUEUE_SIZE-1) ? 0 : q->write+1;
}

/**
 * Process queued bytes into packtes.
 */

inline mpc_pkt* mpc_recv(void) {

	//mpc_master_run();

	while ( !ringbuf_empty(rx_state.buf) )
		mpc_slave_recv_byte(ringbuf_get(rx_state.buf));

	if ( queue_empty(&rx_state.queue.hdr) ) return NULL;

	mpc_pkt * pkt = rx_state.queue.items[ rx_state.queue.hdr.read ];
	queue_rd_idx(&rx_state.queue.hdr);

	return pkt;
}



static inline void mpc_slave_recv_byte(uint8_t data) {
	if ( rx_state.size == 0 ) {
		//fist byte = mpc_pkt.len
		rx_state.pkt = (mpc_pkt*)malloc( sizeof(mpc_pkt) + data );

#ifndef MPC_DISABLE_CRC
		rx_state.crc = mpc_crc(MPC_CRC_SHIFT, data);
#endif
		rx_state.pkt->len = data;
		rx_state.size = 1;

	} else {
		((uint8_t*)rx_state.pkt)[rx_state.size] = data;
#ifndef MPC_DISABLE_CRC
		if ( rx_state.size != offsetof(mpc_pkt,chksum) )
			rx_state.crc = mpc_crc(rx_state.crc,data);
#endif
		rx_state.size++;
	}
}

static inline void mpc_slave_recv_pkt(mpc_pkt * pkt) {

	//before returning the packet, flush the bufffarrrr
	while ( !ringbuf_empty(rx_state.buf) )
		mpc_slave_recv_byte(ringbuf_get(rx_state.buf));

	rx_state.queue.items[ rx_state.queue.hdr.write ] = pkt;
	queue_wr_idx(&rx_state.queue.hdr);
}

MPC_TWI_SLAVE_ISR {

    // If address match. 
	if ( (MPC_TWI.SLAVE.STATUS & TWI_SLAVE_APIF_bm) &&  (MPC_TWI.SLAVE.STATUS & TWI_SLAVE_AP_bm) ) {

#ifdef MPC_TWI_ADDRMASK
		uint8_t addr = MPC_TWI.SLAVE.DATA;
		if ( addr & mpc_twi_addr ) {
#endif
			//basically a hack instead of having per-packet queues.
			while ( !ringbuf_empty(rx_state.buf) )
				mpc_slave_recv_byte(ringbuf_get(rx_state.buf));

			if ( rx_state.pkt != NULL ) 
				free(rx_state.pkt);

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

#ifdef MPC_DISABLE_CRC
		if (1) 
#else
		if ( rx_state.crc == rx_state.pkt->chksum )
#endif
			mpc_slave_recv_pkt(rx_state.pkt);
		else {
			free( rx_state.pkt );
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
			free(rx_state.pkt);
			rx_state.pkt = NULL;
		}
		//todo?
		//TWI_SlaveTransactionFinished(twi, TWIS_RESULT_FAIL);
	}
}


/****
 * END TWI SLAVE CODE - BEGIN MASTER
 * (why did I combine the filez?)
 */

inline void mpc_send_cmd(const uint8_t addr, const uint8_t cmd) {
	mpc_send(addr, cmd, NULL, 0);
}

//CALLER MUST FREE() data*
void mpc_send(const uint8_t addr, const uint8_t cmd, uint8_t * const data, const uint8_t len) {

	mpc_pkt * pkt = (mpc_pkt*)malloc(sizeof(*pkt) + len);

	pkt->len = len;
	pkt->cmd = cmd;
	pkt->saddr = mpc_twi_addr;
	pkt->chksum = MPC_CRC_SHIFT;

#ifndef MPC_DISABLE_CRC
	for ( uint8_t i = 0; i < sizeof(*pkt)-sizeof(pkt->chksum); ++i )
		pkt->chksum = mpc_crc(pkt->chksum, ((uint8_t*)pkt)[i]);
#endif
	for ( uint8_t i = 0; i < len; ++i ) {
			pkt->data[i] = data[i];
#ifndef MPC_DISABLE_CRC
			pkt->chksum = mpc_crc(pkt->chksum, data[i]);
#endif
	}
	
	tx_state.queue.items[ tx_state.queue.hdr.write ].addr = addr; 
	tx_state.queue.items[ tx_state.queue.hdr.write ].pkt = pkt;
	queue_wr_idx(&tx_state.queue.hdr);
	mpc_master_run();
}

static inline void mpc_master_run(void) {

	//notthing to do harr.
	if ( queue_empty(&tx_state.queue.hdr) || tx_state.status != TX_STATUS_IDLE ) return; 

	tx_state.status = TX_STATUS_BUSY;
		
	mpc_pkt * pkt = tx_state.queue.items[ tx_state.queue.hdr.read ].pkt;
	tx_state.pkt = pkt;
	tx_state.len = sizeof(*pkt) + pkt->len;
	tx_state.byte = 0;

	MPC_TWI.MASTER.ADDR = tx_state.queue.items[ tx_state.queue.hdr.read ].addr<<1 | 0;
	queue_rd_idx(&tx_state.queue.hdr);
}

static inline void mpc_master_end_txn(void) {
	free(tx_state.pkt);
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
			tx_state.status = TX_STATUS_IDLE;

			//should move to the next packet.
			mpc_master_run();

		} else {
			if ( tx_state.byte < tx_state.len ) {
				MPC_TWI.MASTER.DATA  = ((uint8_t*)tx_state.pkt)[tx_state.byte++];
			} else {
				MPC_TWI.MASTER.CTRLC = TWI_MASTER_CMD_STOP_gc;
				MPC_TWI.MASTER.STATUS = TWI_MASTER_BUSSTATE_IDLE_gc;
				mpc_master_end_txn();
				mpc_master_run();
			}

		}
	//IDFK??
	} else {
		MPC_TWI.MASTER.CTRLC = TWI_MASTER_CMD_STOP_gc;
		
		//not sure wtf this is, so unlike the other errors, we'll just flush the dataaaa
		mpc_master_end_txn();
	}
}
