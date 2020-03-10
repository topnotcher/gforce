#include <stdint.h>
#include <string.h>
//#include <util/crc16.h>

#include <g4config.h>
#include "config.h"

#include <malloc.h>
#include <mempool.h>
#include <mpc.h>
#include <serialcomm.h>
#include <drivers/sam/sercom.h>
#include <drivers/sam/dma.h>
#include <drivers/sam/isr.h>

#include "xbee.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#define xbee_crc(crc, data) (crc)

static struct xbee_s {
	serialcomm_t *comm;
	mempool_t *mempool;
	QueueHandle_t tx_queue;
	TaskHandle_t tx_task;
	TaskHandle_t rx_task;
	sercom_t sercom;
	int8_t dma_chan;
} xbee;

struct _tx_vec {
	const uint8_t *buf;
	uint8_t size;
	void (*complete)(void *);
};

static void xbee_rx_pkt(mpc_pkt const *const);
static uint8_t xbee_pkt_chksum(mpc_pkt const *const);
static void xbee_rx_process(uint8_t, uint8_t *);
static void xbee_rx_task(void *params);
static void xbee_tx_task(void *params);
static void configure_dma(void);
static void xbee_tx_complete(void);

static void xbee_write_cmd(const char *const);
static void xbee_write_cmd_complete(void *);

static void xbee_write(struct xbee_s *const dev, const uint8_t *const data, const uint8_t len, void (*complete)(void *buf));
static uint8_t xbee_getchar(const struct xbee_s *const dev);

void xbee_init(void) {
	// SERCOM0 MUX: RXPO = 1, TXPO = 2

	// XBEE_~TCP_CONN = PC20: set to input with pullup
	PORT[0].Group[2].DIRCLR.reg = 1 << 20;
	PORT[0].Group[2].PINCFG[20].reg = PORT_PINCFG_PULLEN | PORT_PINCFG_INEN;
	PORT[0].Group[2].OUTSET.reg = 1 << 20;

	// XBEE_~RST = PC21: set to output, hold in reset.
	PORT[0].Group[2].DIRSET.reg = 1 << 21;
	PORT[0].Group[2].OUTCLR.reg = 1 << 21;

	// XBEE_SLEEP_RQ = PA18: set to output, disable sleep.
	PORT[0].Group[0].DIRSET.reg = 1 << 18;
	PORT[0].Group[0].OUTCLR.reg = 1 << 18;

	// XBEE_AWAKE = PA19: set to input, pull low.
	PORT[0].Group[0].DIRCLR.reg = 1 << 19;
	PORT[0].Group[0].PINCFG[19].reg = PORT_PINCFG_PULLEN | PORT_PINCFG_INEN;
	PORT[0].Group[0].OUTCLR.reg = 1 << 19;

	memset(&xbee, 0, sizeof(xbee));
	if (sercom_init(0, &xbee.sercom)) {
		serialcomm_driver driver = {
			.dev = &xbee,
			.rx_func = (uint8_t (*)(void*))xbee_getchar,
			.tx_func = (void (*)(void*, uint8_t *, uint8_t, void (*)(void *)))xbee_write,
		};

		xbee.mempool = init_mempool(MPC_PKT_MAX_SIZE, MPC_QUEUE_SIZE);
		xbee.comm = serialcomm_init(driver, MPC_ADDR_BOARD);
		xbee.tx_queue = xQueueCreate(5, sizeof(struct _tx_vec));
		configure_dma();

		sercom_set_gclk_core(&xbee.sercom, GCLK_PCHCTRL_GEN_GCLK0);
		sercom_set_gclk_slow(&xbee.sercom, GCLK_PCHCTRL_GEN_GCLK0);

		xbee.sercom.hw->USART.CTRLA.reg = SERCOM_USART_CTRLA_SWRST;
		while (xbee.sercom.hw->USART.SYNCBUSY.reg & SERCOM_USART_SYNCBUSY_SWRST);
		uint32_t ctrla = SERCOM_USART_CTRLA_TXPO(2) | SERCOM_USART_CTRLA_RXPO(1) | SERCOM_USART_CTRLA_MODE(1) | SERCOM_USART_CTRLA_DORD;
		uint32_t ctrlb = SERCOM_USART_CTRLB_TXEN | SERCOM_USART_CTRLB_RXEN;
		uint16_t baud = 64697;

		xbee.sercom.hw->USART.CTRLA.reg = ctrla;
		xbee.sercom.hw->USART.CTRLB.reg = ctrlb;
		xbee.sercom.hw->USART.BAUD.reg = baud;

		xbee.sercom.hw->USART.CTRLA.reg |= SERCOM_USART_CTRLA_ENABLE;
		while (xbee.sercom.hw->USART.SYNCBUSY.reg & SERCOM_USART_SYNCBUSY_ENABLE);

		if (xbee.comm != NULL && xbee.mempool != NULL) {
			xTaskCreate(xbee_rx_task, "xbee-rx", 256, NULL, tskIDLE_PRIORITY + 1, &xbee.rx_task);
			xTaskCreate(xbee_tx_task, "xbee-tx", 256, NULL, tskIDLE_PRIORITY + 1, &xbee.tx_task);
		}
	}
}

static void xbee_rx_task(void *params) {

	// assert ~RST
	PORT[0].Group[2].OUTCLR.reg = 1 << 21;

	// Deassert TX - page 48 of XBee DS: this brings it up in command mode with
	// known parameters...
	PORT[0].Group[2].PINCFG[17].reg &= ~PORT_PINCFG_PMUXEN;
	PORT[0].Group[2].DIRSET.reg = 1 << 17;
	PORT[0].Group[2].OUTCLR.reg = 1 << 17;

	// leave RST asserted for 50ms
	vTaskDelay(configTICK_RATE_HZ/20);

	// deassert ~RST
	PORT[0].Group[2].OUTSET.reg = 1 << 21;

	// wait another 50ms (BREAK)
	// TODO: this should wait for the xbee to send OK
	vTaskDelay(configTICK_RATE_HZ/20);

	// reenable MUX for TX PIN.
	PORT[0].Group[2].PINCFG[17].reg |= PORT_PINCFG_PMUXEN;

	//xbee should be in command mode now. Ideally we should wait to read OK\r from xbee.
	// tell the xbee to switch to 750000 baud
	xbee_write_cmd("ATBD0xB71B0,AC\r");
	// TODO read okay?
	vTaskDelay(configTICK_RATE_HZ/20);

	xbee.sercom.hw->USART.CTRLA.reg &= ~SERCOM_USART_CTRLA_ENABLE;
	while (xbee.sercom.hw->USART.SYNCBUSY.reg & SERCOM_USART_SYNCBUSY_ENABLE);
	xbee.sercom.hw->USART.BAUD.reg = 0;

	xbee.sercom.hw->USART.CTRLA.reg |= SERCOM_USART_CTRLA_ENABLE;
	while (xbee.sercom.hw->USART.SYNCBUSY.reg & SERCOM_USART_SYNCBUSY_ENABLE);

	// Enable sleep
	xbee_write_cmd("ATSM4,SO0x40,AC\r");

	// Well... For now.. Since the tcp conn LED is hooked up wrong on the
	// xbee... turn the led on manually to indicate successful configuration.
	xbee_write_cmd("ATP54,CN\r");

	// TODO apply stored config from eeprom?

	while (1) {
		uint8_t *rx_buf =  mempool_alloc(xbee.mempool);
		uint8_t rx_size = serialcomm_recv_frame(xbee.comm, rx_buf, xbee.mempool->block_size);

		xbee_rx_process(rx_size, rx_buf);
	}
}

static void xbee_tx_task(void *params) {
	DmacDescriptor *desc = dma_channel_desc(xbee.dma_chan);

	// TODO: only dma channels 0, 1, 2, 3 have their own interrupt mapping...
	nvic_register_isr(DMAC_0_IRQn + xbee.dma_chan, xbee_tx_complete);
	NVIC_EnableIRQ(DMAC_0_IRQn + xbee.dma_chan);
	DMAC->Channel[xbee.dma_chan].CHINTENSET.reg = DMAC_CHINTENSET_TCMPL;

	while (1) {
		struct _tx_vec vec;
		if (xQueueReceive(xbee.tx_queue, &vec, portMAX_DELAY)) {
			desc->SRCADDR.reg = (uint32_t)vec.buf + vec.size;
			desc->BTCNT.reg = vec.size;

			dma_start_transfer(xbee.dma_chan);

			// wait
			while (xTaskNotifyWait(0, 0, NULL, portMAX_DELAY) != pdTRUE);
			if (vec.complete)
				vec.complete((void*)vec.buf);
		}
	}
}

void xbee_tx_complete(void) {
	DMAC->Channel[xbee.dma_chan].CHINTFLAG.reg |= DMAC_CHINTFLAG_TCMPL;
	xTaskNotifyFromISR(xbee.tx_task, 0, eNoAction, NULL);
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

static void xbee_rx_process(uint8_t size, uint8_t *buf) {
	if (size < sizeof(mpc_pkt))
		goto cleanup;

	mpc_pkt *pkt = (mpc_pkt *)buf;

	if (pkt->chksum != xbee_pkt_chksum(pkt))
	goto cleanup;

	xbee_rx_pkt(pkt);

	cleanup:
		mempool_putref(buf);
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

	} else if (pkt->cmd == MPC_CMD_CONFIG) {
		mpc_send(MPC_ADDR_CHEST | MPC_ADDR_PHASOR | MPC_ADDR_LS | MPC_ADDR_RS | MPC_ADDR_BACK, pkt->cmd, pkt->len, (uint8_t*)pkt->data);
	}
}

static uint8_t xbee_pkt_chksum(mpc_pkt const *const pkt) {
	/*
	uint8_t *data = (uint8_t *)pkt;

	uint8_t crc = xbee_crc(MPC_CRC_SHIFT, data[0]);
	for (uint8_t i = 1; i < sizeof(*pkt) + pkt->len; ++i) {
		if (i != offsetof(mpc_pkt, chksum))
			crc = xbee_crc(crc, data[i]);
	}

	return crc;
	*/
	return 0;
}

static void xbee_write_cmd(const char *const cmd) {
	xbee_write(&xbee, (const uint8_t*)cmd, strlen(cmd), xbee_write_cmd_complete);
	/* xbee_write_cmd_complete(NULL); */
	while (xTaskNotifyWait(0, 0, NULL, portMAX_DELAY) != pdTRUE);
}

static void xbee_write_cmd_complete(void *_buf) {
	// TODO: stupid hack
	xTaskNotify(xbee.rx_task, 0, eNoAction);
}

static void xbee_write(struct xbee_s *const dev, const uint8_t *const data, const uint8_t len, void (*complete)(void *buf)) {
	struct _tx_vec vec = {
		.size = len,
		.buf = data,
		.complete  = complete
	};

	if (!xQueueSend(dev->tx_queue, &vec, portMAX_DELAY) && complete)
		complete((void*)data);
}

static uint8_t xbee_getchar(const struct xbee_s *const dev) {
	vTaskSuspend(NULL);
	return 0;
}

static void configure_dma(void) {
	xbee.dma_chan = dma_channel_alloc();

	if (xbee.dma_chan >= 0) {
		DmacDescriptor *desc = dma_channel_desc(xbee.dma_chan);
		desc->BTCTRL.reg =
			DMAC_BTCTRL_BEATSIZE_BYTE |
			DMAC_BTCTRL_SRCINC |
			DMAC_BTCTRL_BLOCKACT_NOACT; 

		desc->BTCNT.reg = 0;
		desc->DESCADDR.reg = 0;
		desc->DSTADDR.reg = (uint32_t)&xbee.sercom.hw->SPI.DATA.reg;

		DMAC->Channel[xbee.dma_chan].CHCTRLA.reg =
			DMAC_CHCTRLA_TRIGSRC(sercom_dma_tx_trigsrc(&xbee.sercom)) |
			DMAC_CHCTRLA_TRIGACT_BURST; // TODO HACK: -- just changed to compile
	}
}
