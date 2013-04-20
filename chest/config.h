#ifndef BOARD_CONFIG_H


/**
 * Configuration specific to this board as required to work with includes etcetc
 */

/**********************************************
 * Configuration for the LED controllers
 */
#define LED_PORT PORTC
#define LED_SCLK PIN7_bm
#define LED_SOUT PIN5_bm


/*********************************************
 * Configuration for the master<>board MPC bus.
 */

#define MPC_TWI_ADDR 0b0001

#define MPC_TWI TWIC
#define MPC_TWI_SLAVE_ISR ISR(TWIC_TWIS_vect)

#endif
