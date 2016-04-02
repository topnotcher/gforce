#include <malloc.h>
#include "mempool.h"
#include "comm.h"
#include "mpctwi.h"
#include "twi_master.h"
#include "twi_slave.h"
#include <stddef.h>

/**
 * This implements a lower-level TWI driver for use with the higher 
 * level comm driver (comm_driver_t and comm_dev_t.  This driver does
 * not implement reading from a slave; instead, it assumes a multi-master
 * bus and requires that the slaves act as masters to send data.
 */

static void begin_tx(comm_driver_t * comm);

static void twim_txn_complete(void *, int8_t);
static void twis_txn_begin(void *, uint8_t, uint8_t **, uint8_t *);
static void twis_txn_end(void *, uint8_t, uint8_t *, uint8_t);

comm_dev_t * mpctwi_init( TWI_t * twi, const uint8_t addr, const uint8_t mask, const uint8_t baud ) {
	comm_dev_t * comm;
	comm = smalloc(sizeof *comm);

	mpc_twi_dev * mpctwi;
	mpctwi = smalloc(sizeof *mpctwi);
	mpctwi->twi = twi;
	
	comm->dev = mpctwi;
	comm->begin_tx = begin_tx;

	mpctwi->twis = twi_slave_init(&twi->SLAVE, addr, mask, comm, twis_txn_begin, twis_txn_end);
	mpctwi->twim = twi_master_init(&twi->MASTER,baud,comm, twim_txn_complete); 

	return comm;
}

static void twim_txn_complete(void *ins, int8_t status) {
	comm_end_tx(((comm_dev_t *)ins)->comm);
}

/**
 * callback for comm - called when the comm state has been 
 * fully initialized to begin a new transfer
 */

static void begin_tx(comm_driver_t * comm) {
	twi_master_write(((mpc_twi_dev*)comm->dev->dev)->twim, comm_tx_daddr(comm), comm_tx_len(comm), comm_tx_data(comm)); 
}

static void twis_txn_begin(void * ins, uint8_t write, uint8_t ** buf, uint8_t * buf_size) {
	comm_driver_t * comm = ((comm_dev_t*)ins)->comm;
	/**
	 * buf may be non-null when passed (if driver has a partially used buffer)
	 * In this case, comm should still have a reference to it and will reuse it when appropriate.
	 */

	if (write) {
		*buf = NULL;
		/*UUSED*/
	} else { 
		comm_begin_rx(comm);
		*buf = comm_rx_buf(comm);
		*buf_size = comm_rx_max_size(comm); 
	}
}

static void twis_txn_end(void * ins, uint8_t write, uint8_t * buf, uint8_t bytes) {
	comm_driver_t * comm = ((comm_dev_t*)ins)->comm;

	if (write) {
		/*UNUSED*/
	} else {
		comm_rx_end(comm,bytes);
	}
}

