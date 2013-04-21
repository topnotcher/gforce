#ifndef BOARD_CONFIG_H

/**
 * Configuration specific to this board as required to work with includes etcetc
 */

#define LED_PORT PORTC
#define LED_SCLK_PIN 7
#define LED_SOUT_PIN 5


#define LED_RB_SWAP 1 

#define MPC_TWI_ADDR_EEPROM_ADDR 0

#define MPC_TWI TWIC
#define MPC_TWI_SLAVE_ISR ISR(TWIC_TWIS_vect)


#define BUZZ_PORT PORTA
#define BUZZ_PIN PIN3_bm

#endif
