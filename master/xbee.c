#include <stdint.h>
#include <util/crc16.h>

#include <g4config.h>
#include "config.h"

#include <serialcomm.h>
#include <comm.h>
#include <malloc.h>
#include <mempool.h>
#include <mpc.h>
#include <tasks.h>
#include "xbee.h"
#include "util.h"

#define _TXPIN_bm G4_PIN(XBEE_TX_PIN)
#define xbee_crc(crc, data) _crc_ibutton_update(crc, data)

static comm_driver_t *xbee_comm;
static mempool_t *xbee_mempool;

static void tx_interrupt_enable(void);
static void tx_interrupt_disable(void);
static void xbee_rx_event(comm_driver_t *comm);
static void xbee_rx_pkt(mpc_pkt const *const pkt);
static uint8_t xbee_pkt_chksum(mpc_pkt const *const pkt);

void xbee_init(void) {

	XBEE_USART.BAUDCTRLA = (uint8_t)(XBEE_BSEL_VALUE & 0x00FF);
	XBEE_USART.BAUDCTRLB = (XBEE_BSCALE_VALUE << USART_BSCALE_gp)
	                       | (uint8_t)((XBEE_BSEL_VALUE >> 8) & 0x0F);

	XBEE_USART.CTRLC = XBEE_CSRC_VALUE;
	XBEE_USART.CTRLA = XBEE_CSRA_VALUE;
	XBEE_USART.CTRLB = XBEE_CSRB_VALUE;

	//Manual, page 237.
	XBEE_PORT.OUTSET = _TXPIN_bm;
	XBEE_PORT.DIRSET = _TXPIN_bm;
	XBEE_PORT.DIRSET = PIN7_bm /*on/sleep*/ | PIN1_bm /*xbee~RTS*/;
	//xbeesleep - status output on xbee
	XBEE_PORT.DIRCLR = PIN5_bm /*xbeesleep*/ | PIN4_bm /*xbee~CTS*/;
	XBEE_PORT.OUTCLR = PIN1_bm /*~RTS is high => Xbee will not send*/;
	XBEE_PORT.OUTSET = PIN7_bm /*~sleep*/;

	xbee_mempool = init_mempool(MPC_PKT_MAX_SIZE + sizeof(comm_frame_t), MPC_QUEUE_SIZE);
	comm_dev_t *commdev = serialcomm_init(&XBEE_USART.DATA, tx_interrupt_enable, tx_interrupt_disable, MPC_MASTER_ADDR);
	xbee_comm = comm_init(commdev, MPC_MASTER_ADDR, MPC_PKT_MAX_SIZE, xbee_mempool, xbee_rx_event);

}

void xbee_send(const uint8_t cmd, const uint8_t size, uint8_t *data) {
	comm_frame_t *frame = mempool_alloc(xbee_mempool);

	//@TODO
	if (frame == NULL)
		return;

	mpc_pkt *pkt;

	frame->size = sizeof(*pkt) + size;
	frame->daddr = 0;

	pkt = (mpc_pkt *)frame->data;

	pkt->len = size;
	pkt->cmd = cmd;
	pkt->saddr = xbee_comm->addr;
	pkt->chksum = MPC_CRC_SHIFT;

	for (uint8_t i = 0; i < sizeof(*pkt) - sizeof(pkt->chksum); ++i)
		pkt->chksum = xbee_crc(pkt->chksum, ((uint8_t *)pkt)[i]);

	for (uint8_t i = 0; i < size; ++i) {
		pkt->data[i] = data[i];
		pkt->chksum = xbee_crc(pkt->chksum, data[i]);
	}

	comm_send(xbee_comm, mempool_getref(frame));
	mempool_putref(frame);
	comm_tx(xbee_comm);
}

static void tx_interrupt_enable(void) {
	xbee_txc_interrupt_enable();
}

static void tx_interrupt_disable(void) {
	xbee_txc_interrupt_disable();
}

static void xbee_rx_event(comm_driver_t *comm) {
	mpc_rx_xbee();
}

void xbee_rx_process(void) {
	comm_frame_t *frame = comm_rx(xbee_comm);

	if (frame == NULL)
		return;

	if (frame->size < sizeof(mpc_pkt))
		goto cleanup;

	mpc_pkt *pkt = (mpc_pkt *)frame->data;

	if (!xbee_pkt_chksum(pkt))
		goto cleanup;

	xbee_rx_pkt(pkt);

cleanup:
	mempool_putref(frame);
}

static void xbee_rx_pkt(mpc_pkt const *const pkt) {

	if (pkt == NULL) return;

	if (pkt->cmd == 'I') {
		mpc_send(MPC_CHEST_ADDR, 'I', pkt->len, (uint8_t *)pkt->data);

		//hackish thing.
		//receive a "ping" from the xbee means
		//send a ping to the board specified by byte 0
		//of the data. That board should then reply...
	} else if (pkt->cmd == 'P') {
		mpc_send_cmd(pkt->data[0], 'P');

	} else if (pkt->cmd == 'M') {
		mpc_send_cmd(pkt->data[0], 'M');

		// this is nasty ...
		mem_usage_t usage = mem_usage();

		xbee_send('M', sizeof(usage), (uint8_t*)&usage);
	}
}

static uint8_t xbee_pkt_chksum(mpc_pkt const *const pkt) {
	uint8_t *data = (uint8_t *)pkt;

	uint8_t crc = xbee_crc(MPC_CRC_SHIFT, data[0]);
	for (uint8_t i = 1; i < sizeof(*pkt) + pkt->len; ++i) {
		if (i != offsetof(mpc_pkt, chksum))
			crc = xbee_crc(crc, data[i]);
	}

	if (crc != pkt->chksum) {
		return 0;
	} else {
		return 1;
	}
}

XBEE_TXC_ISR {
	serialcomm_tx_isr(xbee_comm);
}

XBEE_RXC_ISR {
	serialcomm_rx_isr(xbee_comm);
}
