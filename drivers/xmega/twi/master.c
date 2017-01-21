#include <string.h>

#include <malloc.h>
#include <drivers/xmega/twi/master.h>

struct _twi_master_s {
	TWI_MASTER_t * twi;
	twi_master_complete_txn_cb txn_complete;
	void (*block)(void);
	void (*resume)(void);

	void * ins;
	uint8_t * txbuf;
	uint8_t txbytes;

	uint8_t * rxbuf;
	uint8_t rxbytes;

	uint8_t bytes;
	uint8_t retries;

	/*currently addressed slave*/
	uint8_t addr;
};

static void twi_master_write_handler(twi_master_t *dev);
static void twi_master_read_handler(twi_master_t *dev);
static void twi_master_txn_complete(twi_master_t *dev, int8_t status);
static uint8_t twi_master_module_index(const TWI_MASTER_t *const);

static inline void twi_master_handler(const uint8_t) __attribute__((always_inline));
static void twi_master_isr(twi_master_t *);

// On devices with two TWIs, C and E are present. D and F additionaly on those
// with 4 instances.
#define TWIC_INST_IDX 0 
#define TWIE_INST_IDX 1 
#define TWID_INST_IDX 2 
#define TWIF_INST_IDX 3 

#ifdef TWIF
#define TWI_INST_NUM 4
#else
#define TWI_INST_NUM 2
#endif

static twi_master_t *twim_dev_table[TWI_INST_NUM];

#ifndef TWI_MASTER_MAX_RETRIES
	#define TWI_MASTER_MAX_RETRIES 3
#endif

#define twi_master_start_txn(dev) (dev)->twi->ADDR = (dev)->addr | ((dev)->txbytes ? 0 : 1)

twi_master_t *twi_master_init(TWI_MASTER_t *twi, const uint8_t baud) {
	twi_master_t *dev = smalloc(sizeof *dev);
	uint8_t twi_idx = twi_master_module_index(twi);

	if (twi_idx < TWI_INST_NUM && dev != NULL) {
		memset(dev, 0, sizeof *dev);
		dev->twi = twi;

		// Register the device in the static dev table for the ISR.
		twim_dev_table[twi_idx] = dev;

		/**
		* Master initialization
		*/
		twi->CTRLA |= TWI_MASTER_INTLVL_MED_gc | TWI_MASTER_ENABLE_bm | TWI_MASTER_WIEN_bm | TWI_MASTER_RIEN_bm;
		twi->BAUD = baud;

		//per AVR1308
		twi->STATUS = TWI_MASTER_BUSSTATE_IDLE_gc;
	}

	return dev;
}

void twi_master_set_blocking(twi_master_t *dev, void (*block_cb)(void), void (*resume_cb)(void)) {
	dev->block = block_cb;
	dev->resume = resume_cb;
}

void twi_master_write(twi_master_t *dev, uint8_t addr, uint8_t len, uint8_t *buf) {
	twi_master_write_read(dev, addr, len, buf, 0, NULL);
}

void twi_master_write_read(twi_master_t *dev, uint8_t addr, uint8_t txbytes, uint8_t *txbuf, uint8_t rxbytes, uint8_t *rxbuf) {

	dev->txbuf = txbuf;
	dev->txbytes = txbytes;

	dev->rxbuf = rxbuf;
	dev->rxbytes = rxbytes;

	dev->bytes = 0;
	dev->retries = 0;
	dev->addr = addr << 1;

	/**
	 * @TODO check if it is busy here?
	 */
	twi_master_start_txn(dev);
	if (dev->block)
		dev->block();
}


void twi_master_read(twi_master_t *dev, uint8_t addr, uint8_t len, uint8_t *buf) {
	twi_master_write_read(dev, addr, 0, NULL, len, buf);
}

void twi_master_isr(twi_master_t *dev) {
	TWI_MASTER_t *twi = (TWI_MASTER_t *)dev->twi;

	if ((twi->STATUS & TWI_MASTER_ARBLOST_bm) || (twi->STATUS & TWI_MASTER_BUSERR_bm)) {
		//per AVR1308 example code.
		twi->STATUS |= TWI_MASTER_ARBLOST_bm;

		/**
		 * According to xmegaA, the master should be smart enough to wait
		 * until bussstate == idle before it tries to restart the transaction.
		 * So in theory, this works. If it doesn't work, chances are the tx.state
		 * will stay BUSY indefinitely.
		 */
		if (dev->retries++ < TWI_MASTER_MAX_RETRIES) {
			dev->bytes = 0;
			twi_master_start_txn(dev);
		} else {
			twi_master_txn_complete(dev, -TWI_MASTER_STATUS_ABBR_LOST);
		}
	} else if (twi->STATUS & TWI_MASTER_WIF_bm) {
		twi_master_write_handler(dev);
	} else if (twi->STATUS & TWI_MASTER_RIF_bm) {
		twi_master_read_handler(dev);
	} else {
		//IDFK?? - unexpected type of interrupt.
		twi->CTRLC = TWI_MASTER_CMD_STOP_gc;
		twi_master_txn_complete(dev, -TWI_MASTER_STATUS_UNKNOWN);
	}
}

static void twi_master_read_handler(twi_master_t *dev) {
	TWI_MASTER_t *twi = (TWI_MASTER_t *)dev->twi;

	if (dev->bytes < dev->rxbytes) {
		dev->rxbuf[dev->bytes++] = twi->DATA;
		twi->CTRLC = TWI_MASTER_CMD_RECVTRANS_gc;
	}

	if (dev->bytes >= dev->rxbytes) {
		twi->CTRLC = TWI_MASTER_ACKACT_bm | TWI_MASTER_CMD_STOP_gc;
		twi_master_txn_complete(dev, TWI_MASTER_STATUS_OK);
	}
}

static void twi_master_write_handler(twi_master_t *dev) {
	TWI_MASTER_t *twi = (TWI_MASTER_t *)dev->twi;

	//when it is read as 0, most recent ack bit was NAK.
	if (twi->STATUS & TWI_MASTER_RXACK_bm) {
		//WHY IS THIS HERE??? <-- what you on about?
		//and why is the busstate manually set?
		twi->STATUS = TWI_MASTER_BUSSTATE_IDLE_gc;
		if (dev->retries++ < TWI_MASTER_MAX_RETRIES) {
			dev->bytes = 0;
			twi_master_start_txn(dev);
		} else {
			twi->CTRLC = TWI_MASTER_CMD_STOP_gc;
			twi_master_txn_complete(dev, -TWI_MASTER_STATUS_SLAVE_NAK);
		}
	} else {
		if (dev->bytes < dev->txbytes) {
			twi->DATA = dev->txbuf[dev->bytes++];
		} else {
			twi->CTRLC = TWI_MASTER_CMD_STOP_gc;
			twi->STATUS = TWI_MASTER_BUSSTATE_IDLE_gc;

			//send repeated start if bytes to read
			if (dev->rxbytes) {
				dev->bytes = 0;
				twi->ADDR = dev->addr | 1;
			} else {
				twi_master_txn_complete(dev, TWI_MASTER_STATUS_OK);
			}
		}
	}
}

void twi_master_set_callback(twi_master_t *dev, void *ins, twi_master_complete_txn_cb txn_complete) {
	if (dev) {
		dev->ins = ins;
		dev->txn_complete = txn_complete;
	}
}

static void twi_master_txn_complete(twi_master_t *dev, int8_t status) {
	if (dev->block) {
		dev->resume();
	} else {
		dev->txn_complete(dev->ins, status);
	}
}

static uint8_t twi_master_module_index(const TWI_MASTER_t *const twim) {
#ifdef TWIC
	if (twim == &TWIC.MASTER)
		return TWIC_INST_IDX;
#endif
#ifdef TWID
	if (twim == &TWID.MASTER)
		return TWID_INST_IDX;
#endif
#ifdef TWIE
	if (twim == &TWIE.MASTER)
		return TWIE_INST_IDX;
#endif
#ifdef TWIF
	if (twim == &TWIF.MASTER)
		return TWIF_INST_IDX;
#endif

	return 255;
}

static void twi_master_handler(const uint8_t twi_idx) {
	twi_master_isr(twim_dev_table[twi_idx]);
}

#define CONCAT(a, b, c) a ## b ## c
#define TWIM_HANDLER(x) void TWI ## x ## TWIM_vect(void) { \
	twi_master_handler(CONCAT(TWI, x, _INST_IDX)); \
}

#ifdef TWIC
TWIM_HANDLER(C)
#endif

#ifdef TWID
TWIM_HANDLER(D)
#endif

#ifdef TWIE
TWIM_HANDLER(E)
#endif

#ifdef TWIF
TWIM_HANDLER(F)
#endif
