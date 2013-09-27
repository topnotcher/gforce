#include <stddef.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/crc16.h>

#include <g4config.h>
#include "config.h"
#include <mpc.h>
#include <util.h>

#include "comm.h"
#include "game.h"
#include "twi.h"

#define mpc_crc(crc,data) _crc_ibutton_update(crc,data)

#include "display.h"

static twi_driver_t * twi;
static chunkpool_t * chunkpool;

/**
 * The shoulders need to differentiate left/right.
 */
#ifdef MPC_TWI_ADDR_EEPROM_ADDR
#include <avr/eeprom.h>
#endif

inline void mpc_init(void) {

	uint8_t mpc_twi_addr;
	uint8_t mask;

	#ifdef MPC_TWI_ADDR_EEPROM_ADDR
		mpc_twi_addr = eeprom_read_byte((uint8_t*)MPC_TWI_ADDR_EEPROM_ADDR)<<1;
	#else
		mpc_twi_addr = ((uint8_t)MPC_TWI_ADDR)<<1;
	#endif

	#ifdef MPC_TWI_ADDRMASK
		mask = MPC_TWI_ADDRMASK;
	#else
		mask = 0;
	#endif

	chunkpool = chunkpool_create(MPC_PKT_MAX_SIZE + sizeof(comm_frame_t), 8);
	twi = twi_init(&MPC_TWI, mpc_twi_addr, mask, MPC_TWI_BAUD, MPC_PKT_MAX_SIZE, chunkpool);
}


/**
 * Process queued bytes into packtes.
 */

void mpc_rx_process(void) {
	
	comm_frame_t * frame;
	mpc_pkt * pkt;

	if ( (frame = twi_rx(twi)) == NULL ) 
		return;
	
	pkt = (mpc_pkt*)frame->data;

#ifndef MPC_DISABLE_CRC 
	uint8_t crc = mpc_crc(MPC_CRC_SHIFT, frame->data[0]);
	for ( uint8_t i = 0; i < frame->size; ++i ) {
		if ( i != offsetof(mpc_pkt, chksum) ) {
			crc = mpc_crc( crc, frame->data[i] );
	}

	if ( crc != pkt->crc ) return;
#endif

	if ( pkt->cmd == 'I' )
		process_ir_pkt(pkt);

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
	pkt->saddr = twi->addr;
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
	
	twi_send(twi, frame);
}

void mpc_tx_process(void) {
	twi_tx(twi);
}

MPC_TWI_MASTER_ISR {
	twi_master_isr(twi);
}

MPC_TWI_SLAVE_ISR {
	twi_slave_isr(twi);
}
