
#ifndef BOARD_CONFIG_H


/**
 * Configuration specific to this board as required to work with includes etcetc
 */

/**********************************************
 * Configuration for the LED controllers
 */
#define LED_PORT PORTC
#define LED_SCLK_PIN 7
#define LED_SOUT_PIN 5


/*********************************************
 * Configuration for the master<>board MPC bus.
 */

#define MPC_TWI_ADDR 0b0001

#define MPC_TWI TWIC
#define MPC_TWI_SLAVE_ISR ISR(TWIC_TWIS_vect)


/********************************************
 * Configuration for the vibrator
 */
#define BUZZ_PORT PORTA
#define BUZZ_PIN PIN3_bm

#endif
