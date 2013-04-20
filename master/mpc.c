#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>

#include "mpc.h"

void mpc_init() {

	TWIC.MASTER.CTRLA |= TWI_MASTER_INTLVL_MED_gc | TWI_MASTER_ENABLE_bm | TWI_MASTER_WIEN_bm;
	/*CTRLB SMEN: look into, but only in master read*/
	/*CTRLC ACKACT ... master read*/
	/*CTRLC: CMD bits are here. HRRERM*/
	/* BAUD: 32000000/(2*100000) - 5 = 155*/
	TWIC.MASTER.BAUD = 35/*155*//*F_CPU/(2*MPU_TWI_BAUD) - 5*/;

	//per AVR1308
	TWIC.MASTER.STATUS = TWI_MASTER_BUSSTATE_IDLE_gc;

/*
	PORTC.DIRSET = PIN0_bm | PIN1_bm;
	PORTC.PIN0CTRL = PORT_OPC_PULLUP_gc;
	PORTC.PIN1CTRL = PORT_OPC_PULLUP_gc;
*/

}	

ISR(TWIC_TWIM_vect) {
	static uint8_t i = 0;
	static uint8_t on = 0;
	static const uint8_t pkt_size = 3;
	static uint8_t pkt[2][3] = {
		{'A','0', 134},
		{'B','0', 122}
	};


    if ( (TWIC.MASTER.STATUS & TWI_MASTER_ARBLOST_bm) || (TWIC.MASTER.STATUS & TWI_MASTER_BUSERR_bm)) {
		TWIC.MASTER.STATUS = TWI_MASTER_BUSSTATE_IDLE_gc;
    }

	else if ( TWIC.MASTER.STATUS & TWI_MASTER_WIF_bm ) {
		//when it is read as 0, most recent ack bit was NAK. 
		if (TWIC.MASTER.STATUS & TWI_MASTER_RXACK_bm) 
			TWIC.MASTER.CTRLC = TWI_MASTER_CMD_STOP_gc;

		else {
			//if bytes to send: send bytes
			//otherweise : twi->interface->MASTER.CTRLC = TWI_MASTER_CMD_STOP_gc

			if ( i < pkt_size ) {
				TWIC.MASTER.DATA  = pkt[on][i++];
			} else {
				TWIC.MASTER.CTRLC = TWI_MASTER_CMD_STOP_gc;
				TWIC.MASTER.STATUS = TWI_MASTER_BUSSTATE_IDLE_gc;	
				i = 0;
				
				if ( on++ == 1 )
					on = 0;
			}

		}

	//IDFK??
	} else {
		TWIC.MASTER.CTRLC = TWI_MASTER_CMD_STOP_gc;

		//fail
		return;
	}
}
