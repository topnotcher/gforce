#include <malloc.h>
#include "chunkpool.h"
#include "comm.h"
#include "twi.h"
#include <stddef.h>

/**
 * This implements a lower-level TWI driver for use with the higher 
 * level comm driver (comm_driver_t and comm_dev_t.  This driver does
 * not implement reading from a slave; instead, it assumes a multi-master
 * bus and requires that the slaves act as masters to send data.
 */

static void begin_tx(comm_driver_t * comm);

comm_dev_t * twi_init( TWI_t * dev, const uint8_t addr, const uint8_t mask, const uint8_t baud ) {
	comm_dev_t * comm;
	comm = smalloc(sizeof *comm);
	
	comm->dev = dev;

	/**
	 * Slave initialization
	 */
	dev->SLAVE.CTRLA = TWI_SLAVE_INTLVL_LO_gc | TWI_SLAVE_ENABLE_bm | TWI_SLAVE_APIEN_bm /*| TWI_SLAVE_PMEN_bm*/;
	dev->SLAVE.ADDR = addr<<1;

	if (mask)
		dev->SLAVE.ADDRMASK = mask << 1;

	/**
	 * Master initialization
	 */
	dev->MASTER.CTRLA |= TWI_MASTER_INTLVL_MED_gc | TWI_MASTER_ENABLE_bm | TWI_MASTER_WIEN_bm;
	dev->MASTER.BAUD = baud;

	//per AVR1308
	dev->MASTER.STATUS = TWI_MASTER_BUSSTATE_IDLE_gc;

	comm->begin_tx = begin_tx;

	return comm;
}

/**
 * callback for comm - called when the comm state has been 
 * fully initialized to begin a new transfer
 */
static void begin_tx(comm_driver_t * comm) {
	((TWI_t*)(comm->dev->dev))->MASTER.ADDR = comm_tx_daddr(comm)<<1 /*| 0*/;
}

void twi_master_isr(comm_driver_t * comm) {
	TWI_t * twi = (TWI_t*)comm->dev->dev;

	if ( (twi->MASTER.STATUS & TWI_MASTER_ARBLOST_bm) || (twi->MASTER.STATUS & TWI_MASTER_BUSERR_bm)) {
		//per AVR1308 example code.
		twi->MASTER.STATUS |= TWI_MASTER_ARBLOST_bm;

		/**
		 * According to xmegaA, the master should be smart enough to wait 
		 * until bussstate == idle before it tries to restart the transaction.
		 * So in theory, this works. If it doesn't work, chances are the tx.state
		 * will stay BUSY indefinitely.
		 */
		comm_tx_rewind(comm);
		twi->MASTER.ADDR = comm_tx_daddr(comm)<<1;

	} else if ( twi->MASTER.STATUS & TWI_MASTER_WIF_bm ) {

		//@TODO should this really drop the packet, or should it retry?
		//when it is read as 0, most recent ack bit was NAK. 
		if (twi->MASTER.STATUS & TWI_MASTER_RXACK_bm) {
			//WHY IS THIS HERE???
			//and why is the busstate manually set?
			twi->MASTER.CTRLC = TWI_MASTER_CMD_STOP_gc;
			twi->MASTER.STATUS = TWI_MASTER_BUSSTATE_IDLE_gc;
			
			comm_end_tx(comm);

		} else {
			if ( comm_tx_has_more(comm) ) {
				twi->MASTER.DATA = comm_tx_next(comm);
			} else {
				twi->MASTER.CTRLC = TWI_MASTER_CMD_STOP_gc;
				twi->MASTER.STATUS = TWI_MASTER_BUSSTATE_IDLE_gc;
				comm_end_tx(comm);
			}
		}
	} else {
		//IDFK?? - unexpected type of interrupt.
		twi->MASTER.CTRLC = TWI_MASTER_CMD_STOP_gc;
		comm_end_tx(comm);
	}
}

void twi_slave_isr(comm_driver_t * comm) {
	TWI_t * twi = (TWI_t*)comm->dev->dev;

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