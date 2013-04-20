#include <stdlib.h>
#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#include "config.h"

#include <util.h>
#include <pkt.h>
#include <ringbuf.h>
#include <mpc.h>
#include <leds.h>
#include <buzz.h>


static void mpc_recv_byte(uint8_t);
static void init_recv(void);
static void recv_pkt(pkt_hdr pkt);



ringbuf_t * recvq;

pkt_proc recv;


/**
 * The shoulders need to differentiate left/right.
 */
#ifdef MPC_TWI_ADDR_EEPROM_ADDR
#include <avr/eeprom.h>
uint8_t mpc_twi_addr ;
#endif

/**
 * Initialize the MPC interface.
 */
void mpc_init() {

	recvq = ringbuf_init(MPC_RECVQ_MAX);
	init_recv();

	MPC_TWI.SLAVE.CTRLA = TWI_SLAVE_INTLVL_LO_gc | TWI_SLAVE_ENABLE_bm | TWI_SLAVE_APIEN_bm;

#ifdef MPC_TWI_ADDR 
	MPC_TWI.SLAVE.ADDR = MPC_TWI_ADDR<<1;
#endif 
#ifdef MPC_TWI_ADDR_EEPROM_ADDR
	mpc_twi_addr = eeprom_read_byte((uint8_t*)MPC_TWI_ADDR_EEPROM_ADDR)<<1;
	MPC_TWI.SLAVE.ADDR = mpc_twi_addr;
#endif 
	MPC_TWI.SLAVE.ADDRMASK = MPC_TWI_ADDRMASK << 1;
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
	if ( pkt.cmd == 'A' ) {
		led_set_seq(pkt.data);
		set_lights(1);
	}
	else if ( pkt.cmd == 'B' ) 
		set_lights(0);
	else if ( pkt.cmd == 'C' ) {
		buzz_on();
	} else if ( pkt.cmd == 'D') {
		buzz_off();
	}
}

MPC_TWI_SLAVE_ISR {
    // If address match. 
	if ( (MPC_TWI.SLAVE.STATUS & TWI_SLAVE_APIF_bm) &&  (MPC_TWI.SLAVE.STATUS & TWI_SLAVE_AP_bm) ) {

		uint8_t addr = MPC_TWI.SLAVE.DATA;

#ifdef MPC_TWI_ADDR
		if ( addr & (MPC_TWI_ADDR<<1) ) {
#else
		if ( addr & mpc_twi_addr ) {
#endif
			//set data interrupt because we actually give a shit
			MPC_TWI.SLAVE.CTRLA |= TWI_SLAVE_DIEN_bm;

			//also a stop interrupt.. Wait that's enabled with APIEN?
			MPC_TWI.SLAVE.CTRLA |= TWI_SLAVE_PIEN_bm;

			//this is SUPER temp.
			recv.size = 0;
//			if ( recv.pkt.data != NULL )
//				free(recv.pkt.data);

//			ringbuf_flush(recvq);

			MPC_TWI.SLAVE.CTRLB = TWI_SLAVE_CMD_RESPONSE_gc;

		} else {
			//is this right?
			MPC_TWI.SLAVE.CTRLB = TWI_SLAVE_CMD_COMPTRANS_gc;
		}

	// data interrupt 
	} else if (MPC_TWI.SLAVE.STATUS & TWI_SLAVE_DIF_bm) {
		// slave write 
		if (MPC_TWI.SLAVE.STATUS & TWI_SLAVE_DIR_bm) {

		//slave read(master write)
		} else {
			mpc_recv_byte(MPC_TWI.SLAVE.DATA);
			MPC_TWI.SLAVE.CTRLB = TWI_SLAVE_CMD_RESPONSE_gc;
//			ringbuf_put(recvq, MPC_TWI.SLAVE.DATA);
		}
	} else if ( MPC_TWI.SLAVE.STATUS & TWI_SLAVE_APIF_bm ) {
		
//		if ( recv.crc == recv.pkt.chksum )
			recv_pkt(recv.pkt);
//		else if ( recv.pkt.data != NULL )
//			free(recv.pkt.data);

	    /* Disable stop interrupt. */
    	MPC_TWI.SLAVE.CTRLA &= ~TWI_SLAVE_PIEN_bm;

    	MPC_TWI.SLAVE.STATUS |= TWI_SLAVE_APIF_bm;

		MPC_TWI.SLAVE.CTRLA &= ~TWI_SLAVE_DIEN_bm;
	}

	// If unexpected state.
	else {
		//todo?
		//TWI_SlaveTransactionFinished(twi, TWIS_RESULT_FAIL);
	}
}
