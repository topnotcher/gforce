#include <stdint.h>
#include <sam.h>

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

void configure_pinmux(void);
void configure_clocks(void);

/**
 * GPIO assignments
 */
typedef enum {
	PIN_SD_POWER_OFF = PIN_PA07,
	PIN_SD_CD = PIN_PA06,

	// Xbee pins
	PIN_XBEE_SLEEP_REQUEST = PIN_PA18,
	PIN_XBEE_AWAKE = PIN_PA19,
	PIN_XBEE_RST = PIN_PC21,
	PIN_XBEE_TCP_CONN = PIN_PC20,
	PIN_XBEE_TX = PIN_PC17,

	// Charger GPIO
	PIN_CHG_GP0 = PIN_PC15,
	PIN_CHG_GP1 = PIN_PC14,
	PIN_CHG_GP2 = PIN_PC13,

	PIN_VIBRATOR = PIN_PA27,

	// Pack power off
	PIN_PACK_POWER_OFF = PIN_PB04,

	// Power switch state
	PIN_PACK_ON = PIN_PB05,

	// External power connected
	PIN_EXTPWR = PIN_PB06,

	// LED controller power switch
	PIN_LED_CONTROLLER_OFF = PIN_PB14,

	// audio mute
	PIN_MUTE = PIN_PB20,

	// Phaser receiver enable
	PIN_PH_RX_DISABLE = PIN_PB17,

} GpioPin;


/**
 * MPC TWI Configuration
 */
#define MPC_TWI TWIC

/**
 * MPC (general) configuration
 */
#define MPC_ADDR_BOARD MPC_ADDR_MASTER
#define MPC_QUEUE_SIZE 12

/**************************************
 * Phasor Communication Configuration
 */
#define PHASOR_COMM_USART USARTE0

/**************************************
 * XBee Configuration
 */
#define XBEE_BSEL_VALUE 12
#define XBEE_BSCALE_VALUE 4

#define XBEE_PORT PORTF

#define XBEE_USART USARTF0


#define XBEE_QUEUE_MAX 25

#define MPC_PROCESS_XBEE 1


#endif
