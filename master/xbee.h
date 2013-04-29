#include <avr/io.h>
#include <avr/interrupt.h>

#include <mpc.h>

#ifndef XBEE_H
#define XBEE_H

/** 
 * Value of baud rate register.
 * (F_CPU/(16LU*XBEE_BAUD_RATE) - 1);
 * #define BAUD_PRESCALE (((F_CPU / (USART_BAUDRATE * 16UL))) - 1) 
 * giant dicks get sucked if I put the expression here. 
 * Integer truncation??
 * BSEL=12, BSCALE =4 <-- damnnnn thats right
 */
#define XBEE_BSEL_VALUE 12
#define XBEE_BSCALE_VALUE 4

#define XBEE_USART USARTF0


#define XBEE_BAUDA USARTF0.BAUDCTRLA

#define XBEE_BAUDB USARTF0.BAUDCTRLB
	
#define XBEE_CSRA USARTF0.CTRLA
#define XBEE_CSRB USARTF0.CTRLB
#define XBEE_CSRC USARTF0.CTRLC

/**
 * (RX|TX)EN rx/tx enable.
 * RXCIE0 RX Complete Interrupt enable 
 * UDRIE0 Data register empty interrupt enable - runs when new data can be added to 
 */
#define XBEE_CSRA_VALUE  USART_RXCINTLVL_MED_gc

/**
 * parity/stop bits
 */
#define XBEE_CSRB_VALUE (USART_RXEN_bm | USART_TXEN_bm)

#define XBEE_CSRC_VALUE USART_PMODE_DISABLED_gc | USART_CHSIZE_8BIT_gc

/**
 * Data register
 */
#define XBEE_UDR USARTF0.DATA


/**
 * Use these when transferring data. 
 * If > 1 bytes, enable interrupt, push bytes on each time interrupt is run.
 * interrupt must be disabled when there are no more bytes.
 */
#define xbee_txc_interrupt_enable() XBEE_CSRA |= USART_DREINTLVL_MED_gc
#define xbee_txc_interrupt_disable() XBEE_CSRA &= ~USART_DREINTLVL_MED_gc


/**
 * ISR for transmitting
 */
#define XBEE_TXC_ISR ISR(USARTF0_DRE_vect)

/**
 * ISR for received bytes
 */
#define XBEE_RXC_ISR ISR(USARTF0_RXC_vect)




#define XBEE_QUEUE_MAX 25


void xbee_init(void);

mpc_pkt * xbee_recv(void);

void xbee_send(const uint8_t cmd, const uint8_t size, uint8_t * data);


#endif
