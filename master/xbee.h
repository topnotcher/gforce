#include <avr/io.h>
#include <avr/interrupt.h>

#include <mpc.h>

#include "config.h"

#ifndef XBEE_H
#define XBEE_H


#define XBEE_BAUDA XBEE_USART.BAUDCTRLA

#define XBEE_BAUDB XBEE_USART.BAUDCTRLB
	
#define XBEE_CSRA XBEE_USART.CTRLA
#define XBEE_CSRB XBEE_USART.CTRLB
#define XBEE_CSRC XBEE_USART.CTRLC

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
 * Use these when transferring data. 
 * If > 1 bytes, enable interrupt, push bytes on each time interrupt is run.
 * interrupt must be disabled when there are no more bytes.
 */
#define xbee_txc_interrupt_enable() XBEE_CSRA |= USART_DREINTLVL_MED_gc
#define xbee_txc_interrupt_disable() XBEE_CSRA &= ~USART_DREINTLVL_MED_gc

void xbee_init(void);

mpc_pkt * xbee_recv(void);

void xbee_send(const uint8_t cmd, const uint8_t size ,uint8_t * data);
void xbee_send_pkt(const mpc_pkt *const spkt);
void xbee_rx_process(uint8_t, uint8_t *);


#endif
