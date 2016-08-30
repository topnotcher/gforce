#ifndef BOARD_CONFIG_H

/**
 * Configuration specific to this board as required to work with includes etcetc
 */

#define LED_PORT PORTC
#define LED_SCLK_PIN 5
#define LED_SOUT_PIN 7
#define LED_USART USARTC1
#define LED_DMA_TRIGSRC DMA_CH_TRIGSRC_USARTC1_DRE_gc

#define MPC_TWI_ADDR_EEPROM_ADDR 0
#define MPC_TWI_ADDRMASK 0x3F

#define MPC_TWI TWIC
#define MPC_TWI_SLAVE_ISR ISR(TWIC_TWIS_vect)
#define MPC_TWI_MASTER_ISR ISR(TWIC_TWIM_vect)


#define BUZZ_PORT PORTA
#define BUZZ_PIN PIN3_bm

/**************************************
 * IR RX configuration
 */
#define IRRX_USART USARTE0
#define IRRX_USART_RXC_vect USARTE0_RXC_vect

#define IRRX_USART_PORT PORTE
#define IRRX_USART_RXPIN 2

#define MPC_QUEUE_SIZE 4

#endif
