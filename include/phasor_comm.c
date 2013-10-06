#include <avr/interrupt.h>

#include <stdint.h>

#include <malloc.h>
#include <comm.h>
#include <serialcomm.h>
#include <util.h>
#include <mpc.h>
#include <chunkpool.h>

#include <g4config.h>
#include "config.h"
#include <phasor_comm.h>

#include <util/crc16.h>
#define mpc_crc(crc,data) _crc_ibutton_update(crc,data)

#define _TXPIN_bm G4_PIN(PHASOR_COMM_USART_TXPIN)
#define _RXPIN_bm G4_PIN(PHASOR_COMM_USART_RXPIN)

static void tx_interrupt_enable(void);
static void tx_interrupt_disable(void);

comm_driver_t * phasor_comm_init(chunkpool_t * chunkpool, uint8_t mpc_addr) {
	comm_dev_t * commdev;

	PHASOR_COMM_USART.BAUDCTRLA = (uint8_t)( PHASOR_COMM_BSEL_VALUE & 0x00FF );
	PHASOR_COMM_USART.BAUDCTRLB = (PHASOR_COMM_BSCALE_VALUE<<USART_BSCALE_gp) | (uint8_t)( (PHASOR_COMM_BSEL_VALUE>>8) & 0x0F ) ;

	PHASOR_COMM_USART.CTRLC |= USART_PMODE_DISABLED_gc | USART_CHSIZE_8BIT_gc;
	PHASOR_COMM_USART.CTRLA |= USART_RXCINTLVL_MED_gc;
	PHASOR_COMM_USART.CTRLB |= USART_RXEN_bm | USART_TXEN_bm;

	//xmegaA, p237
	PHASOR_COMM_USART_PORT.OUTSET = _TXPIN_bm;
	PHASOR_COMM_USART_PORT.DIRSET = _TXPIN_bm;
	
	commdev = serialcomm_init(&PHASOR_COMM_USART.DATA, tx_interrupt_enable, tx_interrupt_disable, mpc_addr);
	return comm_init( commdev, mpc_addr, MPC_PKT_MAX_SIZE, chunkpool );
}


static void tx_interrupt_enable(void) {
	PHASOR_COMM_USART.CTRLA |= USART_DREINTLVL_MED_gc;
}

static void tx_interrupt_disable(void) {
	PHASOR_COMM_USART.CTRLA &= ~USART_DREINTLVL_MED_gc;
}
