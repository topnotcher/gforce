#include <avr/io.h>
#include <g4config.h>
#include <util.h>
#include "config.h"
#include "ds2483.h"

static ds2483_dev_t * onewiredev;

void ibutton_init(void) {
	onewiredev = ds2483_init(&DS2483_TWI.MASTER, MPC_TWI_BAUD, &DS2483_SLPZ_PORT,G4_PIN(DS2483_SLPZ_PIN));

	ds2483_bus_rst(onewiredev);
}
