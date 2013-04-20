#include <stdlib.h>
#include <stdint.h>
#include <avr/interrupt.h>

#include "config.h"

#include <util.h>
#include <pkt.h>
#include <ringbuf.h>
#include <mpc.h>
#include <leds.h>


static void mpc_recv_byte(uint8_t);
static void init_recv(void);
static void recv_pkt(pkt_hdr pkt);


ringbuf_t * recvq;

pkt_proc recv;

/**
 * Initialize the MPC interface.
 */
void mpc_init() {

	recvq = ringbuf_init(MPC_RECVQ_MAX);
	init_recv();

	TWIC.SLAVE.CTRLA = TWI_SLAVE_INTLVL_LO_gc | TWI_SLAVE_ENABLE_bm | TWI_SLAVE_APIEN_bm;
	TWIC.SLAVE.ADDR = MPC_TWI_ADDR<<1;
	TWIC.SLAVE.ADDRMASK = MPC_TWI_ADDRMASK << 1;
}

static inline void init_recv() {
	recv.size = 0;
	recv.pkt.data = NULL;
	recv.crc = MPC_CRC_SHIFT;
}

/**
 * Process queued bytes into packtes.
 */
inline void mpc_recv() {
	while ( !ringbuf_empty(recvq) )
		mpc_recv_byte(ringbuf_get(recvq));
}

static inline void mpc_recv_byte(uint8_t data) {

	if ( recv.size == 0 ) {
		recv.crc = MPC_CRC_SHIFT;
		recv.pkt.cmd = data;
		crc(&recv.crc, data, MPC_CRC_POLY);
		recv.size = 1;

	} else if ( recv.size == 1 ) {
		recv.pkt.len = data;
		crc(&recv.crc, data, MPC_CRC_POLY);
		recv.size = 2;

		if ( recv.pkt.len > 0 )
			recv.pkt.data = (uint8_t *)malloc(recv.pkt.len);

	//case 1: there is no payload, so byte 2 is the CRC
	//case 2: there is a payload, but we're past it, so this is the CRC.
	} else if ( (recv.size == 2 && recv.pkt.len == 0) || (recv.pkt.len > 0 && recv.size >= recv.pkt.len+2) ) {
		recv.pkt.chksum = data;
	
//		if ( recv.pkt.chksum == recv.crc )
//			recv_pkt(recv.pkt);
		//note: the receiving end is required to free() the data otherwise!
//		else if ( recv.pkt.len > 0 ) 
//			free(recv.pkt.data);

//		recv.size = 0;

	//receive the payload.
	} else if ( recv.size >= 2 && recv.size < recv.pkt.len+2 ) {
		recv.pkt.data[recv.size-2] = data;
		crc(&recv.crc, data, MPC_CRC_POLY);
		recv.size++;
	} else {
		recv.size = 0;
	}
}

static inline void recv_pkt(pkt_hdr pkt) {
	if ( pkt.cmd == 'A' )
		set_lights(1);
	else if ( pkt.cmd == 'B' ) 
		set_lights(0);

	if ( pkt.len )
		free(pkt.data);
}




ISR(TWIC_TWIS_vect) {

    // If address match. 
	if ( (TWIC.SLAVE.STATUS & TWI_SLAVE_APIF_bm) &&  (TWIC.SLAVE.STATUS & TWI_SLAVE_AP_bm) ) {

		uint8_t addr = TWIC.SLAVE.DATA;

		if ( addr & (MPC_TWI_ADDR<<1) ) {

			//set data interrupt because we actually give a shit
			TWIC.SLAVE.CTRLA |= TWI_SLAVE_DIEN_bm;

			//also a stop interrupt.. Wait that's enabled with APIEN?
			TWIC.SLAVE.CTRLA |= TWI_SLAVE_PIEN_bm;

			//this is SUPER temp.
			recv.size = 0;
			if ( recv.pkt.data != NULL )
				free(recv.pkt.data);
			ringbuf_flush(recvq);

			TWIC.SLAVE.CTRLB = TWI_SLAVE_CMD_RESPONSE_gc;

		} else {
			//is this right?
			TWIC.SLAVE.CTRLB = TWI_SLAVE_CMD_COMPTRANS_gc;
		}

	}

	// If data interrupt. 
	else if (TWIC.SLAVE.STATUS & TWI_SLAVE_DIF_bm) {
		// slave write 
		if (TWIC.SLAVE.STATUS & TWI_SLAVE_DIR_bm) {

		//slave read(master write)
		} else {
			TWIC.SLAVE.CTRLB = TWI_SLAVE_CMD_RESPONSE_gc;
			ringbuf_put(recvq, TWIC.SLAVE.DATA);
		}
	} else if ( TWIC.SLAVE.STATUS & TWI_SLAVE_APIF_bm ) {

		recv_pkt(recv.pkt);


	    /* Disable stop interrupt. */
    	TWIC.SLAVE.CTRLA &= ~TWI_SLAVE_PIEN_bm;

    	TWIC.SLAVE.STATUS |= TWI_SLAVE_APIF_bm;

		TWIC.SLAVE.CTRLA &= ~TWI_SLAVE_DIEN_bm;
	}

	// If unexpected state.
	else {
		//todo?
		//TWI_SlaveTransactionFinished(twi, TWIS_RESULT_FAIL);
	}
}


