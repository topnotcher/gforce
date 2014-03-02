#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <g4config.h>
#include <util.h>
#include "config.h"
#include "ds2483.h"
#include "ibutton.h"

static ds2483_dev_t * onewiredev;
static void ibutton_txn_complete(ds2483_dev_t * dev, uint8_t status);
static uint8_t i = 0;

void ibutton_init(void) {
	onewiredev = ds2483_init(&DS2483_TWI.MASTER, MPC_TWI_BAUD, &DS2483_SLPZ_PORT,G4_PIN(DS2483_SLPZ_PIN), ibutton_txn_complete);
}

void ibutton_detect_cycle(void) {
	i = 0;
	ds2483_rst(onewiredev);
}

static void ibutton_txn_complete(ds2483_dev_t * dev, uint8_t status) {
	if (i == 0) {
		i++;
		_delay_ms(5);
		ds2483_1w_rst(dev);	
	} else if (i == 1) {
		i++;
		_delay_ms(1);
		ds2483_read_register(dev, DS2483_REGISTER_STATUS);
	} else if (i == 2) {
		++i;
		_delay_ms(1);
		ds2483_1w_write(dev, IBUTTON_CMD_READ_ROM);
	} else if ( i >= 3 && i <= 18) {
		//ODD
		if (i & 1) {
			++i;
			_delay_ms(1);
			ds2483_1w_read(dev);
		} else {
			++i;
			_delay_ms(1);
			ds2483_read_register(dev, DS2483_REGISTER_READ_DATA);
		}
	}
}

DS2483_INTERRUPT_HANDLER(ISR(TWIE_TWIM_vect), onewiredev)
