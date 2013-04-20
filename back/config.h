#ifndef BOARD_CONFIG_H

/**
 * Configuration specific to this board as required to work with includes etcetc
 */

#define LED_PORT PORTC
#define LED_SCLK PIN7_bm
#define LED_SOUT PIN5_bm

#define MPC_TWI_ADDR 0b0100

#define MPC_TWI TWIC
#define MPC_TWI_SLAVE_ISR ISR(TWIC_TWIS_vect)

#define BUZZ_PORT PORTA
#define BUZZ_PIN PIN3_bm

#endif
