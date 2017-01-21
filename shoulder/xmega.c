#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>

#include <mpc.h>
#include <mpctwi.h>
#include <leds.h>
#include <buzz.h>
#include <irrx.h>
#include <diag.h>

#include <leds.h>
#include <drivers/xmega/led_drivers.h>
#include <drivers/xmega/clock.h>
#include <drivers/xmega/twi/master.h>
#include <drivers/xmega/twi/slave.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <g4config.h>
#include "config.h"

#include "main.h"

static led_spi_dev *led_spi;

system_init_func(system_board_init) {
	sysclk_set_internal_32mhz();

	// Prescale system clock /2
	uint8_t val = CLK.PSCTRL & ~CLK_PSADIV_gm;
	val |= CLK_PSADIV2_bm;

	CCP = CCP_IOREG_gc;
	CLK.PSCTRL = val;

	_Static_assert(configCPU_CLOCK_HZ == 16000000, "configCPU_CLOCK_HZ != 32MHz");
}

system_init_func(system_configure_interrupts) {
	PMIC.CTRL |= PMIC_LOLVLEN_bm | PMIC_MEDLVLEN_bm | PMIC_HILVLEN_bm;
}

system_init_func(system_software_init) {
	mpc_init();
	buzz_init();
	diag_init();

	led_init();
	irrx_init();
}

void mpc_register_drivers(void) {
	uint8_t twi_addr = eeprom_read_byte((uint8_t *)MPC_TWI_ADDR_EEPROM_ADDR);

	twi_slave_t *twis = twi_slave_init(&MPC_TWI.SLAVE, twi_addr, MPC_TWI_ADDRMASK);
	twi_master_t *twim = twi_master_init(&MPC_TWI.MASTER, MPC_TWI_BAUD);

	mpc_register_driver(mpctwi_init(twim, twis, twi_addr, MPC_ADDR_MASTER));
}

led_spi_dev *led_init_driver(void) {
	return (led_spi = led_usart_init(&USARTC1, 0));
}
