#include <stdint.h>
#include <sam.h>

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

void configure_pinmux(void);

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
