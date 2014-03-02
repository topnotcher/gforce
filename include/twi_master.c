#include <util.h>
#include <malloc.h>
#include <twi_master.h>

static inline void twi_master_write_handler(twi_master_t * dev) ATTR_ALWAYS_INLINE;
static inline void twi_master_read_handler(twi_master_t * dev) ATTR_ALWAYS_INLINE;

twi_master_t * twi_master_init(
			TWI_MASTER_t * twi, 
			const uint8_t baud, 
			void * ins,
			void (* txn_complete)(void *, uint8_t)
) {
	twi_master_t * dev;
	dev = smalloc(sizeof *dev);

	dev->twi = twi;
	dev->txn_complete = txn_complete;
	dev->ins = ins;

	/**
	 * Master initialization
	 */
	twi->CTRLA |= TWI_MASTER_INTLVL_MED_gc | TWI_MASTER_ENABLE_bm | TWI_MASTER_WIEN_bm | TWI_MASTER_RIEN_bm;
	twi->BAUD = baud;

	//per AVR1308
	twi->STATUS = TWI_MASTER_BUSSTATE_IDLE_gc;

	return dev;
}

void twi_master_write(twi_master_t * dev, uint8_t addr, uint8_t len, uint8_t * buf) {
	twi_master_write_read(dev,addr,len,buf,0,NULL);	
}
   
void twi_master_write_read(twi_master_t * dev, uint8_t addr, uint8_t txbytes, uint8_t * txbuf, uint8_t rxbytes, uint8_t * rxbuf) {

	dev->txbuf = txbuf;
	dev->txbytes = txbytes;

	dev->rxbuf = rxbuf;
	dev->rxbytes = rxbytes;

	dev->bytes = 0;
	dev->addr = addr<<1;

	/**
	 * @TODO check if it is busy here?
	 */
	dev->twi->ADDR = dev->addr | (dev->txbytes ? 0 : 1);
}


void twi_master_read(twi_master_t * dev, uint8_t addr, uint8_t len, uint8_t * buf) {
	twi_master_write_read(dev,addr,0,NULL,len,buf);
}
	
void twi_master_isr(twi_master_t * dev) {
	TWI_MASTER_t * twi = (TWI_MASTER_t*)dev->twi;

	if ( (twi->STATUS & TWI_MASTER_ARBLOST_bm) || (twi->STATUS & TWI_MASTER_BUSERR_bm)) {
		//per AVR1308 example code.
		twi->STATUS |= TWI_MASTER_ARBLOST_bm;

		/**
		 * According to xmegaA, the master should be smart enough to wait 
		 * until bussstate == idle before it tries to restart the transaction.
		 * So in theory, this works. If it doesn't work, chances are the tx.state
		 * will stay BUSY indefinitely.
		 */
		dev->bytes = 0;
		twi->ADDR = dev->addr | (dev->txbytes ? 0 : 1);

	} else if ( twi->STATUS & TWI_MASTER_WIF_bm ) {
		twi_master_write_handler(dev);
	} else if (twi->STATUS & TWI_MASTER_RIF_bm) {
		twi_master_read_handler(dev);
	} else {
		//IDFK?? - unexpected type of interrupt.
		twi->CTRLC = TWI_MASTER_CMD_STOP_gc;
		dev->txn_complete(dev->ins,0);
	}
}

static inline void twi_master_read_handler(twi_master_t * dev) {
	TWI_MASTER_t * twi = (TWI_MASTER_t*)dev->twi;

	if (dev->bytes < dev->rxbytes) {
		dev->rxbuf[dev->bytes++] = twi->DATA;
		twi->CTRLC = TWI_MASTER_CMD_RECVTRANS_gc;
	}

	if (dev->bytes >= dev->rxbytes) { 
		twi->CTRLC = TWI_MASTER_ACKACT_bm | TWI_MASTER_CMD_STOP_gc;
		dev->txn_complete(dev->ins, 0);
	}
}

static inline void twi_master_write_handler(twi_master_t * dev) {
	TWI_MASTER_t * twi = (TWI_MASTER_t*)dev->twi;

	//@TODO should this really drop the packet, or should it retry?
	//when it is read as 0, most recent ack bit was NAK. 
	if (twi->STATUS & TWI_MASTER_RXACK_bm) {
		dev->txn_complete(dev->ins,0);

		//WHY IS THIS HERE???
		//and why is the busstate manually set?
		twi->CTRLC = TWI_MASTER_CMD_STOP_gc;
		twi->STATUS = TWI_MASTER_BUSSTATE_IDLE_gc;
	} else {
		if ( dev->bytes < dev->txbytes ) {
			twi->DATA = dev->txbuf[dev->bytes++];
		} else {
			twi->CTRLC = TWI_MASTER_CMD_STOP_gc;
			twi->STATUS = TWI_MASTER_BUSSTATE_IDLE_gc;
			
			//send repeated start if bytes to read
			if (dev->rxbytes) {
				dev->bytes = 0;
				twi->ADDR = dev->addr | 1;
			} else {
				dev->txn_complete(dev->ins, 0);
			}
		}
	}
}
