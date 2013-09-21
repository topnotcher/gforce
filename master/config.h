#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

//last 4 bits = reserved for "slave" boardz
#define MPC_TWI_ADDR 0x40
#define MPC_TWI TWIC
#define MPC_TWI_MASTER_ISR ISR(TWIC_TWIM_vect)
#define MPC_TWI_SLAVE_ISR ISR(TWIC_TWIS_vect)


/**************************************
 * Phasor Communication Configuration
 */
#define PHASOR_COMM_USART USARTE0
#define PHASOR_COMM_USART_PORT PORTE
#define PHASOR_COMM_USART_TXPIN 3
#define PHASOR_COMM_USART_RXPIN 2

#define PHASOR_COMM_TXC_ISR ISR(USARTE0_DRE_vect)
#define PHASOR_COMM_RXC_ISR ISR(USARTE0_RXC_vect)

#define MALLOC_HEAP_SIZE 3072

#endif
