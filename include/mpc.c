#include <stdint.h>
#include <avr/interrupt.h>

#include "config.h"

#include <mpc.h>
#include <leds.h>

void mpc_init() {

	TWIC.SLAVE.CTRLA = TWI_SLAVE_INTLVL_LO_gc | TWI_SLAVE_ENABLE_bm | TWI_SLAVE_APIEN_bm;
	TWIC.SLAVE.ADDR = MPC_TWI_ADDR<<1;
	TWIC.SLAVE.ADDRMASK = MPC_TWI_ADDRMASK << 1;

	while(1) {
		leds_run();
	}

	return 0;
}

ISR(TWIC_TWIS_vect) {

    // If address match. 
	if ( (TWIC.SLAVE.STATUS & TWI_SLAVE_APIF_bm) &&  (TWIC.SLAVE.STATUS & TWI_SLAVE_AP_bm) ) {
		//TWIC.MASTER.CTRLA &= ~TWI_SLAVE_PIEN_bm;

		//clear APIF: should stop clock from being held low????
//		TWIC.SLAVE.STATUS |= TWI_SLAVE_APIF_bm;

		uint8_t addr = TWIC.SLAVE.DATA;

		if ( addr & (MPC_TWI_ADDR<<1) ) {
			TWIC.SLAVE.CTRLB = TWI_SLAVE_CMD_RESPONSE_gc;

			//set data interrupt because we actually give a shit
			TWIC.SLAVE.CTRLA |= TWI_SLAVE_DIEN_bm;

			//also a stop interrupt.. Wait that's enabled with APIEN?
			TWIC.SLAVE.CTRLA |= TWI_SLAVE_PIEN_bm;
		} else {
			//is this right?
			TWIC.SLAVE.CTRLB = TWI_SLAVE_CMD_COMPTRANS_gc;
		}

	}

	// If data interrupt. 
	else if (TWIC.SLAVE.STATUS & TWI_SLAVE_DIF_bm) {
		// slave write 
		if (TWIC.SLAVE.STATUS & TWI_SLAVE_DIR_bm) {

		//slave read(master write)
		} else {
			TWIC.SLAVE.CTRLA |= TWI_SLAVE_PIEN_bm;
			TWIC.SLAVE.CTRLB = TWI_SLAVE_CMD_RESPONSE_gc;
			uint8_t data = TWIC.SLAVE.DATA;

			if ( data == 'A'  ) {
				set_lights(1);
			} else if ( data == 'B' )
				set_lights(0);
		}
	} else if ( TWIC.SLAVE.STATUS & TWI_SLAVE_APIF_bm ) {
	    /* Disable stop interrupt. */
    	TWIC.SLAVE.CTRLA &= ~TWI_SLAVE_PIEN_bm;

    	TWIC.SLAVE.STATUS |= TWI_SLAVE_APIF_bm;

		TWIC.SLAVE.CTRLA &= ~TWI_SLAVE_DIEN_bm;
	}

	// If unexpected state. 
	else {
		//TWI_SlaveTransactionFinished(twi, TWIS_RESULT_FAIL);
	}
}


