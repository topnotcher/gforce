#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>

#include <util.h>

#include <comm.h>
#include <serialcomm.h>
#include <mempool.h>
#include <displaycomm.h>

#include "display.h"
#include "mastercomm.h"

#define COMM_SPI SPID
#define COMM_PORT PORTD
#define COMM_SPI_vect SPID_INT_vect

#define COMM_PIN_SOUT 6
#define COMM_PIN_SIN 5
#define COMM_PIN_SCLK 7
#define COMM_PIN_SS 4

#define _SCLK_bm G4_PIN(COMM_PIN_SCLK)
#define _SIN_bm G4_PIN(COMM_PIN_SIN)
#define _SOUT_bm G4_PIN(COMM_PIN_SOUT)
#define _SS_bm G4_PIN(COMM_PIN_SS)

static comm_driver_t * comm;

void dummy(void);

inline void mastercomm_init(void) {
	
	//xmegaA, pp226.
	COMM_PORT.DIRCLR = _SCLK_bm | _SS_bm | _SIN_bm;

	//not even using, but meh.
	COMM_PORT.DIRSET = _SOUT_bm;
	COMM_PORT.OUTSET = _SOUT_bm;


	COMM_SPI.CTRL = SPI_ENABLE_bm; /*SPI_MASTER_bm=0*/
	COMM_SPI.INTCTRL = SPI_INTLVL_LO_gc;

//	comm_uart_driver = uart_init(&COMM_SPI.DATA, dummy,dummy, RX_BUFF_SIZE);

	mempool_t * mempool = init_mempool(DISPLAY_PKT_MAX_SIZE + sizeof(comm_frame_t), 2);
	comm_dev_t * commdev = serialcomm_init(&COMM_SPI.DATA, dummy, dummy, 1 /*dummy address*/);
	comm = comm_init( commdev, 1 /*dummy address*/, DISPLAY_PKT_MAX_SIZE, mempool ,NULL);

}

void dummy(void) {}

inline display_pkt * mastercomm_recv(void) {
	comm_frame_t * frame;

	//@TODO this is backwards. Shouldn't be decrementing the refcnt so early
	//but it should work almost all of the time, and I'm being lazy at the moment.	
	if ( (frame = comm_rx(comm)) != NULL ) {
		mempool_putref(frame);
		return (display_pkt*)frame->data;
	}

	return NULL;
}

ISR(COMM_SPI_vect) {
	serialcomm_rx_isr(comm);
}
