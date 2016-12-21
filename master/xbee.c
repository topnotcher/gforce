#include <stdint.h>
#include <string.h>
#include <util/crc16.h>

#include <g4config.h>
#include "config.h"

#include "uart.h"
#include <serialcomm.h>
#include <comm.h>
#include <malloc.h>
#include <mempool.h>
#include <mpc.h>
#include "xbee.h"
#include "util.h"

#define xbee_crc(crc, data) _crc_ibutton_update(crc, data)

static comm_driver_t *xbee_comm;
static mempool_t *xbee_mempool;
static uart_dev_t *xbee_uart_dev;

static void xbee_rx_event(comm_driver_t *comm);
static void xbee_rx_pkt(mpc_pkt const *const pkt);
static inline uint8_t xbee_pkt_chksum(mpc_pkt const *const pkt);

void xbee_init(void) {
	xbee_uart_dev = uart_init(&XBEE_USART, MPC_QUEUE_SIZE * 2, 24, XBEE_BSEL_VALUE, XBEE_BSCALE_VALUE);

	XBEE_PORT.DIRSET = PIN7_bm /*on/sleep*/ | PIN1_bm /*xbee~RTS*/;
	//xbeesleep - status output on xbee
	XBEE_PORT.DIRCLR = PIN5_bm /*xbeesleep*/ | PIN4_bm /*xbee~CTS*/;
	XBEE_PORT.OUTCLR = PIN1_bm /*~RTS is high => Xbee will not send*/;
	XBEE_PORT.OUTSET = PIN7_bm /*~sleep*/;

	serialcomm_driver driver = {
		.dev = xbee_uart_dev,
		.rx_func = (uint8_t (*)(void*))uart_getchar,
		.tx_func = (void (*)(void *, uint8_t *, uint8_t, void (*)(void *)))uart_write,
	};

	xbee_mempool = init_mempool(MPC_PKT_MAX_SIZE + sizeof(comm_frame_t), MPC_QUEUE_SIZE);
	comm_dev_t *commdev = serialcomm_init(driver, MPC_ADDR_MASTER);
	xbee_comm = comm_init(
			commdev, MPC_ADDR_MASTER, MPC_PKT_MAX_SIZE,
			xbee_mempool, xbee_rx_event,
			MPC_QUEUE_SIZE, MPC_QUEUE_SIZE);
}

void xbee_send_pkt(const mpc_pkt *const spkt) {
	comm_frame_t *frame = mempool_alloc(xbee_mempool);

	//@TODO
	if (frame == NULL)
		return;

	mpc_pkt *pkt;

	frame->size = sizeof(*pkt) + spkt->len;
	frame->daddr = 0;

	pkt = (mpc_pkt *)frame->data;
	memcpy(pkt, spkt, sizeof(*spkt) + spkt->len);

	// TODO: crc code is disabled on all other boards
	pkt->chksum = xbee_pkt_chksum(pkt);

	comm_send(xbee_comm, mempool_getref(frame));
	mempool_putref(frame);
	comm_tx(xbee_comm);
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

	memcpy(&pkt->data, data, size);  // TODO validate size
	pkt->chksum = xbee_pkt_chksum(pkt);

	comm_send(xbee_comm, mempool_getref(frame));
	mempool_putref(frame);
	comm_tx(xbee_comm);
}

static void xbee_rx_event(comm_driver_t *comm) {
	mpc_rx_xbee();
}

void xbee_rx_process(void) {
	comm_frame_t *frame;

	while ((frame = comm_rx(xbee_comm)) != NULL) {

		if (frame->size < sizeof(mpc_pkt))
			goto cleanup;

		mpc_pkt *pkt = (mpc_pkt *)frame->data;

		if (pkt->chksum != xbee_pkt_chksum(pkt))
			goto cleanup;

		xbee_rx_pkt(pkt);

		cleanup:
			mempool_putref(frame);
	}
}

static void xbee_rx_pkt(mpc_pkt const *const pkt) {

	if (pkt == NULL) return;

	// IR echo
	if (pkt->cmd == MPC_CMD_IR_RX) {
		// first address is the board to send it to.
		mpc_send(pkt->data[0], MPC_CMD_IR_RX, pkt->len - 1, (uint8_t *)pkt->data + 1);

		//hackish thing.
		//receive a "ping" from the xbee means
		//send a ping to the board specified by byte 0
		//of the data. That board should then reply...
	} else if (pkt->cmd == MPC_CMD_DIAG_PING) {
		if (pkt->data[0] & MPC_ADDR_MASTER)
			xbee_send(MPC_CMD_DIAG_RELAY, 0, NULL);

		if (pkt->data[0] != MPC_ADDR_MASTER)
			mpc_send_cmd(pkt->data[0] & ~MPC_ADDR_MASTER, MPC_CMD_DIAG_PING);

	} else if (pkt->cmd == MPC_CMD_DIAG_MEM_USAGE) {
		if (pkt->data[0] & MPC_ADDR_MASTER) {
			mem_usage_t usage = mem_usage();
			xbee_send(MPC_CMD_DIAG_MEM_USAGE, sizeof(usage), (uint8_t*)&usage);
		}

		if (pkt->data[0] != MPC_ADDR_MASTER)
			mpc_send_cmd(pkt->data[0] & ~MPC_ADDR_MASTER, MPC_CMD_DIAG_MEM_USAGE);

	} else if (pkt->cmd == MPC_CMD_LED_SET_BRIGHTNESS) {
		mpc_send(MPC_ADDR_CHEST | MPC_ADDR_PHASOR | MPC_ADDR_LS | MPC_ADDR_RS | MPC_ADDR_BACK, pkt->cmd, pkt->len, (uint8_t*)pkt->data);
	}
}

static inline uint8_t xbee_pkt_chksum(mpc_pkt const *const pkt) {
	uint8_t *data = (uint8_t *)pkt;

	uint8_t crc = xbee_crc(MPC_CRC_SHIFT, data[0]);
	for (uint8_t i = 1; i < sizeof(*pkt) + pkt->len; ++i) {
		if (i != offsetof(mpc_pkt, chksum))
			crc = xbee_crc(crc, data[i]);
	}

	return crc;
}

XBEE_TXC_ISR {
	uart_tx_isr(xbee_uart_dev);
}

XBEE_RXC_ISR {
	uart_rx_isr(xbee_uart_dev);
}
