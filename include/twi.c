#include <malloc.h>
#include "chunkpool.h"
#include "comm.h"
#include "twi.h"
#include "twi_master.h"
#include <stddef.h>

/**
 * This implements a lower-level TWI driver for use with the higher 
 * level comm driver (comm_driver_t and comm_dev_t.  This driver does
 * not implement reading from a slave; instead, it assumes a multi-master
 * bus and requires that the slaves act as masters to send data.
 */

static void begin_tx(comm_driver_t * comm);

static void twim_txn_complete(void *, uint8_t);

comm_dev_t * twi_init( TWI_t * twi, const uint8_t addr, const uint8_t mask, const uint8_t baud ) {
	comm_dev_t * comm;
	comm = smalloc(sizeof *comm);

	mpc_twi_dev * mpctwi;
	mpctwi = smalloc(sizeof *mpctwi);
	mpctwi->twi = twi;
	
	comm->dev = mpctwi;

	/**
	 * Slave initialization
	 */
	twi->SLAVE.CTRLA = TWI_SLAVE_INTLVL_LO_gc | TWI_SLAVE_ENABLE_bm | TWI_SLAVE_APIEN_bm /*| TWI_SLAVE_PMEN_bm*/;
	twi->SLAVE.ADDR = addr<<1;

	if (mask)
		twi->SLAVE.ADDRMASK = mask << 1;

	/**
	 * Master initialization
	 */
/*	dev->MASTER.CTRLA |= TWI_MASTER_INTLVL_MED_gc | TWI_MASTER_ENABLE_bm | TWI_MASTER_WIEN_bm;
	dev->MASTER.BAUD = baud;

	//per AVR1308
	dev->MASTER.STATUS = TWI_MASTER_BUSSTATE_IDLE_gc;
*/ 
	comm->begin_tx = begin_tx;

	mpctwi->twim = twi_master_init(&twi->MASTER,baud,comm, twim_txn_complete); 

	return comm;
}

static void twim_txn_complete(void *ins, uint8_t status) {
	comm_end_tx(((comm_dev_t *)ins)->comm);
}
/**
 * callback for comm - called when the comm state has been 
 * fully initialized to begin a new transfer
 */

static void begin_tx(comm_driver_t * comm) {
	twi_master_write(((mpc_twi_dev*)comm->dev->dev)->twim, comm_tx_daddr(comm), comm->tx.frame->size, comm->tx.frame->data); 
}


void twi_slave_isr(comm_driver_t * comm) {
	TWI_t * twi = (TWI_t*)((mpc_twi_dev*)comm->dev->dev)->twi;

	if ( (twi->SLAVE.STATUS & TWI_SLAVE_APIF_bm) &&  (twi->SLAVE.STATUS & TWI_SLAVE_AP_bm) ) {

		uint8_t addr = twi->SLAVE.DATA;
		if ( addr & (comm->addr<<1) ) {
			comm_begin_rx(comm);

			twi->SLAVE.CTRLA |= TWI_SLAVE_DIEN_bm | TWI_SLAVE_PIEN_bm;
			twi->SLAVE.CTRLB = TWI_SLAVE_CMD_RESPONSE_gc;
		} else {
			//is this right?
			twi->SLAVE.CTRLB = TWI_SLAVE_CMD_COMPTRANS_gc;
		}
	} else if (twi->SLAVE.STATUS & TWI_SLAVE_DIF_bm) {
		// slave write 
		if (twi->SLAVE.STATUS & TWI_SLAVE_DIR_bm) {

		//slave read(master write)
		} else {
			if ( !comm_rx_full(comm) )
				comm_rx_byte(comm,twi->SLAVE.DATA);
				
			//@TODO SMART MODE
			twi->SLAVE.CTRLB = TWI_SLAVE_CMD_RESPONSE_gc;
		}
	} else if ( twi->SLAVE.STATUS & TWI_SLAVE_APIF_bm ) {
		comm_end_rx(comm);
    	twi->SLAVE.CTRLA &= ~(TWI_SLAVE_PIEN_bm | TWI_SLAVE_DIEN_bm);
    	twi->SLAVE.STATUS |= TWI_SLAVE_APIF_bm;
	} else {
		//Some kind of unexpected interrupt
	}
}
