#include <avr/interrupt.h>

#include <stdint.h>

#include <malloc.h>
#include <comm.h>
#include <serialdev.h>
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

#define PHASOR_COMM_QUEUE_MAX 5

static comm_driver_t * comm_driver;
static chunkpool_t * chunkpool;

static void tx_interrupt_enable(void);
static void tx_interrupt_disable(void);

inline void phasor_comm_init(void) {
	comm_dev_t * commdev;

	PHASOR_COMM_USART.BAUDCTRLA = (uint8_t)( PHASOR_COMM_BSEL_VALUE & 0x00FF );
	PHASOR_COMM_USART.BAUDCTRLB = (PHASOR_COMM_BSCALE_VALUE<<USART_BSCALE_gp) | (uint8_t)( (PHASOR_COMM_BSEL_VALUE>>8) & 0x0F ) ;

	PHASOR_COMM_USART.CTRLC |= USART_PMODE_DISABLED_gc | USART_CHSIZE_8BIT_gc;
	PHASOR_COMM_USART.CTRLA |= USART_RXCINTLVL_MED_gc;
	PHASOR_COMM_USART.CTRLB |= USART_RXEN_bm | USART_TXEN_bm;

	//xmegaA, p237
	PHASOR_COMM_USART_PORT.OUTSET = _TXPIN_bm;
	PHASOR_COMM_USART_PORT.DIRSET = _TXPIN_bm;
	
	chunkpool = chunkpool_create(MPC_PKT_MAX_SIZE + sizeof(comm_frame_t), 3);

	commdev = serialdev_init(&PHASOR_COMM_USART.DATA, tx_interrupt_enable, tx_interrupt_disable);
	comm_driver = comm_init( commdev, MPC_ADDR, MPC_PKT_MAX_SIZE, chunkpool );
}

void phasor_comm_send( const uint8_t cmd, const uint8_t len, uint8_t * const data) {

	comm_frame_t * frame;
	mpc_pkt * pkt;

	frame = (comm_frame_t*)chunkpool_acquire(chunkpool);

	frame->daddr = 0;/**@TODO*/
	frame->size = sizeof(*pkt)+len;

	pkt = (mpc_pkt*)frame->data;

	pkt->len = len;
	pkt->cmd = cmd;
	//@TODO
	pkt->saddr = comm_driver->addr;
	pkt->chksum = MPC_CRC_SHIFT;

//#ifndef MPC_DISABLE_CRC
	for ( uint8_t i = 0; i < sizeof(*pkt)-sizeof(pkt->chksum); ++i )
		pkt->chksum = mpc_crc(pkt->chksum, ((uint8_t*)pkt)[i]);
//#endif
	for ( uint8_t i = 0; i < len; ++i ) {
			pkt->data[i] = data[i];
//#ifndef MPC_DISABLE_CRC
			pkt->chksum = mpc_crc(pkt->chksum, data[i]);
//#endif
	}
	
	//@TODO FIX THIS
	comm_send(comm_driver, frame);
	chunkpool_decref(frame);
}

void phasor_tx_process(void) {
	comm_tx(comm_driver);
}

inline mpc_pkt * phasor_comm_recv(void) {
	return (mpc_pkt *) NULL;
//	return uart_rx(comm_uart_driver);
}

static void tx_interrupt_enable(void) {
	PHASOR_COMM_USART.CTRLA |= USART_DREINTLVL_MED_gc;
}

static void tx_interrupt_disable(void) {
	PHASOR_COMM_USART.CTRLA &= ~USART_DREINTLVL_MED_gc;
}

PHASOR_COMM_TXC_ISR {
	serialdev_tx_isr(comm_driver);
}

//PHASOR_COMM_RXC_ISR {
//	uart_rx_byte(comm_uart_driver);
//}
