#include <stddef.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/crc16.h>

#include <g4config.h>
#include "config.h"
#include <mpc.h>
#include <util.h>

#include "comm.h"
#include "twi.h"

#define mpc_crc(crc,data) _crc_ibutton_update(crc,data)

static comm_driver_t * comm;

#ifdef PHASOR_COMM_USART
#define PHASOR_COMM 1
#include <phasor_comm.h>
#include "serialcomm.h"
static comm_driver_t * phasor_comm;
#endif

static chunkpool_t * chunkpool;

//@TODO hard-coded # of elementsghey
#define N_MPC_CMDS 10
typedef struct {
	uint8_t cmd;
	void (* cb)(const mpc_pkt * const);
} cmd_callback_s;

static cmd_callback_s cmds[N_MPC_CMDS]; 
static uint8_t next_mpc_cmd = 0;

/**
 * The shoulders need to differentiate left/right.
 */
#ifdef MPC_TWI_ADDR_EEPROM_ADDR
#include <avr/eeprom.h>
#endif

inline void mpc_init(void) {

	uint8_t mpc_addr;
	uint8_t mask;

	#ifdef MPC_TWI_ADDR_EEPROM_ADDR
		mpc_addr = eeprom_read_byte((uint8_t*)MPC_TWI_ADDR_EEPROM_ADDR);
	#else
		mpc_addr = ((uint8_t)MPC_ADDR);
	#endif

	#ifdef MPC_TWI_ADDRMASK
		mask = MPC_TWI_ADDRMASK;
	#else
		mask = 0;
	#endif

	chunkpool = chunkpool_create(MPC_PKT_MAX_SIZE + sizeof(comm_frame_t), MPC_QUEUE_SIZE);

	#ifdef MPC_TWI
	comm_dev_t * twi;
	twi = twi_init(&MPC_TWI, mpc_addr, mask, MPC_TWI_BAUD);
	comm = comm_init(twi, mpc_addr, MPC_PKT_MAX_SIZE, chunkpool);
	#endif

	#ifdef PHASOR_COMM
	phasor_comm = phasor_comm_init(chunkpool);
	#endif
}

/**
 * @TODO this will silently fail on table full
 *
 */
void mpc_register_cmd(const uint8_t cmd, void (*cb)(const mpc_pkt * const)) {
	cmds[next_mpc_cmd].cmd = cmd;
	cmds[next_mpc_cmd].cb = cb;
	next_mpc_cmd++;
}

/**
 * Process queued bytes into packtes.
 */

void mpc_rx_process(void) {
	
	comm_frame_t * frame;
	mpc_pkt * pkt;

	if ( (frame = comm_rx(comm)) == NULL ) 
		return;
	
	pkt = (mpc_pkt*)frame->data;

#ifndef MPC_DISABLE_CRC 
	uint8_t crc = mpc_crc(MPC_CRC_SHIFT, frame->data[0]);
	for ( uint8_t i = 0; i < frame->size; ++i ) {
		if ( i != offsetof(mpc_pkt, chksum) ) {
			crc = mpc_crc( crc, frame->data[i] );
	}

	if ( crc != pkt->chksum ) {
		chunkpool_decref(frame);
		return;
		//	goto: cleanup;
	}
#endif

	for ( uint8_t i = 0; i < next_mpc_cmd; ++i ) {
		if ( cmds[i].cmd == pkt->cmd ) {
			cmds[i].cb(pkt);
			break;
		}
	}

	//cleanup:
	chunkpool_decref(frame);
}

inline void mpc_send_cmd(const uint8_t addr, const uint8_t cmd) {
	mpc_send(addr, cmd, 0, NULL);
}

//CALLER MUST FREE() data*
void mpc_send(const uint8_t addr, const uint8_t cmd, const uint8_t len, uint8_t * const data) {

	comm_frame_t * frame;
	mpc_pkt * pkt;

	frame = (comm_frame_t*)chunkpool_acquire(chunkpool);

	frame->daddr = addr;
	frame->size = sizeof(*pkt)+len;

	pkt = (mpc_pkt*)frame->data;

	pkt->len = len;
	pkt->cmd = cmd;
	//@TODO
	pkt->saddr = comm->addr;
	pkt->chksum = MPC_CRC_SHIFT;

//#ifndef MPC_DISABLE_CRC
	for ( uint8_t i = 0; i < sizeof(*pkt)-sizeof(pkt->chksum); ++i )
		pkt->chksum = mpc_crc(pkt->chksum, ((uint8_t*)pkt)[i]);
//#endif
	for ( uint8_t i = 0; i < len; ++i ) {
			pkt->data[i] = data[i];
//#ifndef MPC_DISABLE_CRC
			pkt->chksum = mpc_crc(pkt->chksum, data[i]);
//#endif
	}
	

	#ifdef MPC_TWI
	if ( addr & (MPC_MASTER_ADDR | MPC_CHEST_ADDR | MPC_LS_ADDR | MPC_RS_ADDR | MPC_BACK_ADDR) )
		comm_send(comm, frame);
	#endif
	
	#ifdef PHASOR_COMM
	if ( addr & (MPC_MASTER_ADDR | MPC_PHASOR_ADDR) )
		comm_send(phasor_comm,frame);
	#endif

	chunkpool_decref(frame);
}

void mpc_tx_process(void) {

	#ifdef MPC_TWI
	comm_tx(comm);
	#endif

	#ifdef PHASOR_COMM
	comm_tx(phasor_comm);
	#endif
}

#ifdef PHASOR_COMM_TXC_ISR
PHASOR_COMM_TXC_ISR {
	serialcomm_tx_isr(phasor_comm);
}
#endif

#ifdef MPC_TWI_MASTER_ISR
MPC_TWI_MASTER_ISR {
	twi_master_isr(comm);
}
#endif
#ifdef MPC_TWI_SLAVE_ISR
MPC_TWI_SLAVE_ISR {
	twi_slave_isr(comm);
}
#endif
