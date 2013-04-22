#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>

#include <string.h> //memset
#include <stdlib.h> //malloc

#include <g4config.h>
#include "config.h"
#include <util.h>
#include <pkt.h>
#include "mpc_master.h"

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



static void mpc_end_txn(void);

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
	mpc_qitem_t items[MPC_QUEUE_SIZE];
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

//@TODO Y U NO ENUM
#define MPC_TX_STATE_IDLE 0
#define MPC_TX_STATE_BUSY 1

mpc_tx_state_t tx_state;

void mpc_master_init() {

	MPC_TWI.MASTER.CTRLA |= TWI_MASTER_INTLVL_MED_gc | TWI_MASTER_ENABLE_bm | TWI_MASTER_WIEN_bm;
	/*CTRLB SMEN: look into, but only in master read*/
	/*CTRLC ACKACT ... master read*/
	/*CTRLC: CMD bits are here. HRRERM*/
	/* BAUD: 32000000/(2*100000) - 5 = 155*/
	MPC_TWI.MASTER.BAUD = MPC_TWI_BAUD/*155*//*F_CPU/(2*MPU_TWI_BAUD) - 5*/;

	//per AVR1308
	MPC_TWI.MASTER.STATUS = TWI_MASTER_BUSSTATE_IDLE_gc;

#ifdef MPC_TWI_ADDR_EEPROM_ADDR
	mpc_twi_addr = eeprom_read_byte((uint8_t*)MPC_TWI_ADDR_EEPROM_ADDR)<<1;
#endif
	
	memset(&queue ,0,sizeof queue);
	memset(&tx_state,0 , sizeof tx_state);
}

void mpc_master_send_cmd(const uint8_t addr, const uint8_t cmd) {
	mpc_master_send(addr, cmd, NULL, 0);
}

//CALLER MUST FREE() data*
void mpc_master_send(const uint8_t addr, const uint8_t cmd, uint8_t * const data, const uint8_t len) {
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

	queue.items[queue.write].addr = addr; 
	queue.items[queue.write].len = len+pkt_hdr_size+pkt_tail_size; //data+header
	queue.items[queue.write].data = raw;

	//w000000....
	//h00000....
	queue.write = (queue.write == MPC_QUEUE_SIZE-1) ? 0 : queue.write+1;

}

void mpc_master_run() {

	//notthing to do harr.
	if ( queue.read == queue.write || tx_state.state != MPC_TX_STATE_IDLE ) return; 

	tx_state.state = MPC_TX_STATE_BUSY;
		
	tx_state.data = queue.items[queue.read].data;
	tx_state.len = queue.items[queue.read].len;
	tx_state.byte = 0;

	MPC_TWI.MASTER.ADDR = queue.items[queue.read].addr<<1 | 0;
	
	queue.read = (queue.read == MPC_QUEUE_SIZE-1) ? 0 : queue.read+1;	

}

void mpc_end_txn(void) {
	if ( tx_state.len > 0 )
		free(tx_state.data);
	tx_state.state = MPC_TX_STATE_IDLE;
}

MPC_TWI_MASTER_ISR  {

    if ( (MPC_TWI.MASTER.STATUS & TWI_MASTER_ARBLOST_bm) || (MPC_TWI.MASTER.STATUS & TWI_MASTER_BUSERR_bm)) {
		/** Go back to the first byte and try the transaction over. 
		 * @TODO: What is a BUSERROR? And what happens if the slave receives part data? CRC would help here. 
		 **/
		tx_state.byte = 0;
		tx_state.state = MPC_TX_STATE_IDLE;

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
			tx_state.state = MPC_TX_STATE_IDLE;

		} else {
			if ( tx_state.byte < tx_state.len ) {
				MPC_TWI.MASTER.DATA  = tx_state.data[tx_state.byte++];
			} else {
				MPC_TWI.MASTER.CTRLC = TWI_MASTER_CMD_STOP_gc;
				MPC_TWI.MASTER.STATUS = TWI_MASTER_BUSSTATE_IDLE_gc;
				mpc_end_txn();
			}

		}
	//IDFK??
	} else {
		MPC_TWI.MASTER.CTRLC = TWI_MASTER_CMD_STOP_gc;
		
		//not sure wtf this is, so unlike the other errors, we'll just flush the dataaaa
		mpc_end_txn();
	}
}
