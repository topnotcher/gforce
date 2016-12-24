#include <stdint.h>
#include <string.h>
#include <util/crc16.h>

#include <g4config.h>
#include "config.h"

#include "uart.h"
#include <serialcomm.h>
#include <malloc.h>
#include <mempool.h>
#include <mpc.h>
#include "xbee.h"
#include "util.h"

#include "FreeRTOS.h"
#include "task.h"

#define xbee_crc(crc, data) _crc_ibutton_update(crc, data)

static struct {
	serialcomm_t *comm;
	mempool_t *mempool;
	uart_dev_t *uart;
} xbee;

static inline void xbee_rx_pkt(mpc_pkt const *const);
static inline uint8_t xbee_pkt_chksum(mpc_pkt const *const);
static inline void xbee_rx_process(uint8_t, uint8_t *);
static void xbee_rx_task(void *params);

void xbee_init(void) {
	xbee.uart = uart_init(&XBEE_USART, MPC_QUEUE_SIZE * 2, 24, XBEE_BSEL_VALUE, XBEE_BSCALE_VALUE);

	XBEE_PORT.DIRSET = PIN7_bm /*on/sleep*/ | PIN1_bm /*xbee~RTS*/;
	//xbeesleep - status output on xbee
	XBEE_PORT.DIRCLR = PIN5_bm /*xbeesleep*/ | PIN4_bm /*xbee~CTS*/;
	XBEE_PORT.OUTCLR = PIN1_bm /*~RTS is high => Xbee will not send*/;
	XBEE_PORT.OUTSET = PIN7_bm /*~sleep*/;

	serialcomm_driver driver = {
		.dev = xbee.uart,
		.rx_func = (uint8_t (*)(void*))uart_getchar,
		.tx_func = (void (*)(void *, uint8_t *, uint8_t, void (*)(void *)))uart_write,
	};

	xbee.mempool = init_mempool(MPC_PKT_MAX_SIZE, MPC_QUEUE_SIZE);
	xbee.comm = serialcomm_init(driver, MPC_ADDR_MASTER);

	xTaskCreate(xbee_rx_task, "xbee-rx", 128, NULL, tskIDLE_PRIORITY + 1, (TaskHandle_t*)NULL);
}

static void xbee_rx_task(void *params) {

	while (1) {
		uint8_t *rx_buf =  mempool_alloc(xbee.mempool);
		uint8_t rx_size = serialcomm_recv_frame(xbee.comm, rx_buf, xbee.mempool->block_size);

		xbee_rx_process(rx_size, rx_buf);
	}
}

void xbee_send_pkt(const mpc_pkt *const spkt) {
	mpc_pkt *pkt = NULL;
	uint8_t pkt_size = sizeof(*pkt) + spkt->len;

	if (pkt_size <= xbee.mempool->block_size)
		pkt = mempool_alloc(xbee.mempool);

	if (pkt != NULL) {
		memcpy(pkt, spkt, pkt_size);

		// TODO: crc code is disabled on all other boards
		pkt->chksum = xbee_pkt_chksum(pkt);

		// ref to frame is given to serialcomm
		serialcomm_send(xbee.comm, 0, pkt_size, (uint8_t*)pkt, mempool_putref);
	}
}

void xbee_send(const uint8_t cmd, const uint8_t size, uint8_t *data) {
	mpc_pkt *pkt = NULL;
	uint8_t pkt_size = sizeof(*pkt) + size;

	if (pkt_size <= xbee.mempool->block_size)
		pkt = mempool_alloc(xbee.mempool);

	if (pkt != NULL) {
		pkt->len = size;
		pkt->cmd = cmd;
		pkt->saddr = MPC_ADDR_MASTER;

		memcpy(&pkt->data, data, size);
		pkt->chksum = xbee_pkt_chksum(pkt);

		// ref to frame is given to serialcomm
		serialcomm_send(xbee.comm, 0, pkt_size, (uint8_t*)pkt, mempool_putref);
	}
}

static inline void xbee_rx_process(uint8_t size, uint8_t *buf) {
	if (size < sizeof(mpc_pkt))
		goto cleanup;

	mpc_pkt *pkt = (mpc_pkt *)buf;

	if (pkt->chksum != xbee_pkt_chksum(pkt))
	goto cleanup;

	xbee_rx_pkt(pkt);

	cleanup:
		mempool_putref(buf);
}

static inline void xbee_rx_pkt(mpc_pkt const *const pkt) {

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
	uart_tx_isr(xbee.uart);
}

XBEE_RXC_ISR {
	uart_rx_isr(xbee.uart);
}
