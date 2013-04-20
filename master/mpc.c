#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>

#include <string.h> //memset
#include <stdlib.h> //malloc

#include <util.h>
#include <pkt.h>
#include "mpc.h"

static void mpc_send_pkt(const uint8_t addr, const pkt_hdr * const pkt);

/**
 * Wrapper structure to hold queued packets.
 */
typedef struct {
	uint8_t addr; 
	uint8_t len;
	uint8_t * data;
} mpc_qitem_t;

/**
 * define a basic ring buffer. SSH
 */
#define MPC_QUEUE_SIZE 8
typedef struct {
	uint8_t read;
	uint8_t write;
	mpc_qitem_t * items[MPC_QUEUE_SIZE];
} mpc_queue_t;

mpc_queue_t queue;


/** 
 * keep track of the state of teh sender.
 */
typedef struct {
	//current byte in transaction
	uint8_t byte;
	//total to write in transaction
	uint8_t len;
	uint8_t * data;
	uint8_t state;
} mpc_tx_state_t;

#define MPC_TX_STATE_IDLE 0
#define MPC_TX_STATE_BUSY 1

mpc_tx_state_t tx_state;

void mpc_init() {

	TWIC.MASTER.CTRLA |= TWI_MASTER_INTLVL_MED_gc | TWI_MASTER_ENABLE_bm | TWI_MASTER_WIEN_bm;
	/*CTRLB SMEN: look into, but only in master read*/
	/*CTRLC ACKACT ... master read*/
	/*CTRLC: CMD bits are here. HRRERM*/
	/* BAUD: 32000000/(2*100000) - 5 = 155*/
	TWIC.MASTER.BAUD = 35/*155*//*F_CPU/(2*MPU_TWI_BAUD) - 5*/;

	//per AVR1308
	TWIC.MASTER.STATUS = TWI_MASTER_BUSSTATE_IDLE_gc;

	memset(&queue ,0,sizeof queue);
	memset(&tx_state,0 , sizeof tx_state);
}

void mpc_send_cmd(const uint8_t addr, const uint8_t cmd) {
	mpc_send(addr, cmd, NULL, 0);
}

void mpc_send(const uint8_t addr, const uint8_t cmd, uint8_t * const data, const uint8_t len) {

	//len = data, +3 for cmd,len,chksum.
	uint8_t * const raw = (uint8_t*)malloc(len+3) ;
	uint8_t chksum = MPC_CRC_SHIFT;

	raw[0] = cmd;
	raw[1] = len;

	crc(&chksum, cmd, MPC_CRC_POLY);
	crc(&chksum, len, MPC_CRC_POLY);


	for ( uint8_t i = 2; i < len+2; ++i ) {
		raw[i] = data[i];
		crc(&chksum, data[i], MPC_CRC_POLY);
	}
	//0    1   2 3 4 5 6  7
	//cmd,len, 1,2,3,4,5, chksum
	raw[len+2] = chksum;

	free(data);

	mpc_qitem_t * qpkt = (mpc_qitem_t*)malloc(sizeof(mpc_qitem_t));
	qpkt->addr = addr; 
	qpkt->len = len+3; //data+header
	qpkt->data = raw;

	
	queue.items[queue.write] = qpkt;
	//w000000....
	//h00000....
	queue.write = (queue.write == MPC_QUEUE_SIZE-1) ? 0 : queue.write+1;

}

void mpc_run() {

	//notthing to do harr.
	if ( queue.read == queue.write || tx_state.state != MPC_TX_STATE_IDLE ) return; 

	tx_state.state = MPC_TX_STATE_BUSY;
		
	tx_state.data = queue.items[queue.read]->data;
	tx_state.len = queue.items[queue.read]->len;
	tx_state.byte = 0;

	free(queue.items[queue.read]);


	queue.read = (queue.read == MPC_QUEUE_SIZE-1) ? 0 : queue.read+1;

}
ISR(TWIC_TWIM_vect) {
	static uint8_t i = 0;
//	static uint8_t on = 0;
//	static const uint8_t pkt_size = 3;
//	static uint8_t pkt[2][12] = {
		static uint8_t pkt[12] = {65,9,1,1,19,87,66,134,10,15,148};
	/*	{'B',0, 122, 0, 0, 0, 0, 0, 0, 0, 0, 0}*/
	//};


    if ( (TWIC.MASTER.STATUS & TWI_MASTER_ARBLOST_bm) || (TWIC.MASTER.STATUS & TWI_MASTER_BUSERR_bm)) {
		TWIC.MASTER.STATUS = TWI_MASTER_BUSSTATE_IDLE_gc;
    }

	else if ( TWIC.MASTER.STATUS & TWI_MASTER_WIF_bm ) {
		//when it is read as 0, most recent ack bit was NAK. 
		if (TWIC.MASTER.STATUS & TWI_MASTER_RXACK_bm) 
			TWIC.MASTER.CTRLC = TWI_MASTER_CMD_STOP_gc;

		else {
			//if bytes to send: send bytes
			//otherweise : twi->interface->MASTER.CTRLC = TWI_MASTER_CMD_STOP_gc

			if ( i < 12 /*pkt_size+pkt[1]*/ ) {
				TWIC.MASTER.DATA  = pkt[i++];
			} else {
				TWIC.MASTER.CTRLC = TWI_MASTER_CMD_STOP_gc;
				TWIC.MASTER.STATUS = TWI_MASTER_BUSSTATE_IDLE_gc;	
				i = 0;
				
//				if ( on++ == 1 )
//					on = 0;
			}

		}

	//IDFK??
	} else {
		TWIC.MASTER.CTRLC = TWI_MASTER_CMD_STOP_gc;

		//fail
		return;
	}
}
