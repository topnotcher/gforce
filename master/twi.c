#include <malloc.h>
#include "chunkpool.h"
#include "comm.h"
#include "twi.h"
#include <stddef.h>

twi_driver_t * twi_init( 
			TWI_t * dev, 
			const uint8_t addr, 
			const uint8_t mask, 
			const uint8_t baud, 
			const uint8_t mtu, 
			chunkpool_t * pool ) {

	twi_driver_t * twi;
	twi = smalloc(sizeof *twi);

	twi->dev = dev;
	twi->addr = addr;
	twi->mtu = mtu;
	twi->pool = pool;

	/**
	 * Slave initialization
	 */
	twi->rx.queue = queue_create(4);
	dev->SLAVE.CTRLA = TWI_SLAVE_INTLVL_LO_gc | TWI_SLAVE_ENABLE_bm | TWI_SLAVE_APIEN_bm /*| TWI_SLAVE_PMEN_bm*/;

	dev->SLAVE.ADDR = addr;

	if (mask)
		dev->SLAVE.ADDRMASK = mask << 1;

	/**
	 * Master initialization
	 */
	twi->tx.state = TWI_TX_STATE_IDLE;
	twi->tx.queue = queue_create(4);

	dev->MASTER.CTRLA |= TWI_MASTER_INTLVL_MED_gc | TWI_MASTER_ENABLE_bm | TWI_MASTER_WIEN_bm;
	dev->MASTER.BAUD = baud;

	//per AVR1308
	dev->MASTER.STATUS = TWI_MASTER_BUSSTATE_IDLE_gc;

	return twi;
}

/**
 * note: this assumes that * frame must have been allocated by the chunk pool allocator
 */
void twi_send(twi_driver_t * twi, comm_frame_t * frame) {
	//@todo log failure?
	queue_offer( twi->tx.queue, (void*)frame );

}

comm_frame_t * twi_rx(twi_driver_t * twi) {
	return queue_poll(twi->rx.queue);
}

/**
 * task to initiate TWI transfers.
 */
void twi_tx(twi_driver_t * twi) {
	comm_frame_t * frame;
	
	if ( twi->tx.state != TWI_TX_STATE_IDLE ) 
		return;

	if ( (frame = queue_poll(twi->tx.queue)) != NULL ) {
		//safe to modify and idle status indicates that the entire tx state is safe to modify.
		twi->tx.state = TWI_TX_STATE_BUSY;
		twi->tx.frame = frame;
		twi->tx.byte = 0;
		twi->dev->MASTER.ADDR = frame->daddr<<1 /*| 0*/;
	}
}

void twi_master_isr(twi_driver_t * twi) {

	if ( (twi->dev->MASTER.STATUS & TWI_MASTER_ARBLOST_bm) || (twi->dev->MASTER.STATUS & TWI_MASTER_BUSERR_bm)) {
		/*
		 * @TODO: Ideally this should restart the transmission 
		 * ... which requires blocking on the bus state. The master SHOULD be 
		 * 	smart enough to do this..
		 **/

		//per AVR1308 example code.
		twi->dev->MASTER.STATUS |= TWI_MASTER_ARBLOST_bm;

		//now in theory it will restart the transaction when the bus becomes idle? theory...
		twi->tx.byte = 0;
		twi->dev->MASTER.ADDR = twi->tx.frame->daddr<<1;

	} else if ( twi->dev->MASTER.STATUS & TWI_MASTER_WIF_bm ) {

		//@TODO should this really drop the packet, or should it retry?
		//when it is read as 0, most recent ack bit was NAK. 
		if (twi->dev->MASTER.STATUS & TWI_MASTER_RXACK_bm) {
			//WHY IS THIS HERE???
			//and why is the busstate manually set?
			twi->dev->MASTER.CTRLC = TWI_MASTER_CMD_STOP_gc;
			twi->dev->MASTER.STATUS = TWI_MASTER_BUSSTATE_IDLE_gc;
			
			if (twi->tx.frame != NULL) {
				chunkpool_release(twi->tx.frame);
				twi->tx.frame = NULL;
			}
		
			twi->tx.state = TWI_TX_STATE_IDLE;
		} else {
			if ( twi->tx.byte < twi->tx.frame->size ) {
				twi->dev->MASTER.DATA = twi->tx.frame->data[twi->tx.byte++];
			} else {
				twi->dev->MASTER.CTRLC = TWI_MASTER_CMD_STOP_gc;
				twi->dev->MASTER.STATUS = TWI_MASTER_BUSSTATE_IDLE_gc;

				chunkpool_release(twi->tx.frame);
				twi->tx.frame = NULL;

				twi->tx.state = TWI_TX_STATE_IDLE;
			}
		}
	//IDFK??
	} else {
		twi->dev->MASTER.CTRLC = TWI_MASTER_CMD_STOP_gc;

		if (twi->tx.frame != NULL) {
			chunkpool_release(twi->tx.frame);
			twi->tx.frame = NULL;
		}

		twi->tx.state = TWI_TX_STATE_IDLE;

	}
}



void twi_slave_isr(twi_driver_t * twi) {

    // If address match. 
	if ( (twi->dev->SLAVE.STATUS & TWI_SLAVE_APIF_bm) &&  (twi->dev->SLAVE.STATUS & TWI_SLAVE_AP_bm) ) {

//#ifdef MPC_TWI_ADDRMASK
		uint8_t addr = twi->dev->SLAVE.DATA;
		if ( addr & twi->addr ) {
//#endif
	
			//just in case - but if it is not-null, then there's an
			//in complete transaction. it's probably garbage, so drop it.
			//@TODO acquire may fail.
			if ( twi->rx.frame == NULL ) 
				twi->rx.frame = (comm_frame_t*)chunkpool_acquire(twi->pool);
			
			twi->rx.frame->size = 0;
	
			twi->dev->SLAVE.CTRLA |= TWI_SLAVE_DIEN_bm | TWI_SLAVE_PIEN_bm;
			twi->dev->SLAVE.CTRLB = TWI_SLAVE_CMD_RESPONSE_gc;

//#ifdef MPC_TWI_ADDRMASK
		} else {
			//is this right?
			twi->dev->SLAVE.CTRLB = TWI_SLAVE_CMD_COMPTRANS_gc;
		}
//#endif

	// data interrupt 
	} else if (twi->dev->SLAVE.STATUS & TWI_SLAVE_DIF_bm) {
		// slave write 
		if (twi->dev->SLAVE.STATUS & TWI_SLAVE_DIR_bm) {

		//slave read(master write)
		} else {
			
			//if it exceeds MTU, it gets dropped when the next start is received.
			if ( twi->rx.frame->size < twi->mtu )
				twi->rx.frame->data[twi->rx.frame->size++] = twi->dev->SLAVE.DATA;
				
			//@TODO SMART MODE
			twi->dev->SLAVE.CTRLB = TWI_SLAVE_CMD_RESPONSE_gc;
		}
	} else if ( twi->dev->SLAVE.STATUS & TWI_SLAVE_APIF_bm ) {
		queue_offer(twi->rx.queue, (void*)twi->rx.frame);	
		twi->rx.frame = NULL;

    	twi->dev->SLAVE.CTRLA &= ~(TWI_SLAVE_PIEN_bm | TWI_SLAVE_DIEN_bm);
    	twi->dev->SLAVE.STATUS |= TWI_SLAVE_APIF_bm;
	}

	// If unexpected state.
	else {
		//todo?
		//TWI_SlaveTransactionFinished(twi, TWIS_RESULT_FAIL);
	}
}
