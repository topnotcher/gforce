#include <stdint.h>

#include "uart.h"
#include "xbee.h"

#define _TXPIN_bm G4_PIN(XBEE_TX_PIN)

static uart_driver_t * xbee_uart_driver;

static void tx_interrupt_enable(void);
static void tx_interrupt_disable(void);

inline void xbee_init(void) {

	XBEE_BAUDA = (uint8_t)( XBEE_BSEL_VALUE & 0x00FF );
	XBEE_BAUDB = (XBEE_BSCALE_VALUE<<USART_BSCALE_gp) | (uint8_t)( (XBEE_BSEL_VALUE>>8) & 0x0F ) ;

	XBEE_CSRC = XBEE_CSRC_VALUE;
	XBEE_CSRA = XBEE_CSRA_VALUE;	
	XBEE_CSRB = XBEE_CSRB_VALUE;


	//Manual, page 237.
	XBEE_PORT.OUTSET = _TXPIN_bm;
	XBEE_PORT.DIRSET = _TXPIN_bm;
	XBEE_PORT.DIRSET = PIN7_bm /*on/sleep*/ | PIN1_bm /*xbee~RTS*/;
	//xbeesleep - status output on xbee
	XBEE_PORT.DIRCLR = PIN5_bm /*xbeesleep*/ | PIN4_bm /*xbee~CTS*/;
	XBEE_PORT.OUTCLR = PIN1_bm /*~RTS is high => Xbee will not send*/;
	XBEE_PORT.OUTSET = PIN7_bm /*~sleep*/;

	xbee_uart_driver = uart_init(&XBEE_USART.DATA, tx_interrupt_enable, tx_interrupt_disable, XBEE_QUEUE_MAX);
}

inline void xbee_send(const uint8_t cmd,const uint8_t size, uint8_t * data) {
	uart_tx(xbee_uart_driver, cmd, size, data);
}

inline mpc_pkt * xbee_recv(void) {
	return uart_rx(xbee_uart_driver);
}

static void tx_interrupt_enable(void) {
	XBEE_USART.CTRLA |= USART_DREINTLVL_MED_gc;
}

static void tx_interrupt_disable(void) {
	XBEE_USART.CTRLA &= ~USART_DREINTLVL_MED_gc;
}

XBEE_TXC_ISR {
	usart_tx_process(xbee_uart_driver);
}

XBEE_RXC_ISR {
	uart_rx_byte(xbee_uart_driver);
}
