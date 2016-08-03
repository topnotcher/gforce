#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include <string.h>

#include <util.h>
#include <string.h>
#include <comm.h>
#include <serialcomm.h>
#include <mempool.h>
#include <displaycomm.h>

#include "FreeRTOS.h"
#include "task.h"

#include "display.h"

#define DISPLAY_SPI SPIC
#define DISPLAY_PORT PORTC
#define DISPLAY_PIN_SOUT 5
#define DISPLAY_PIN_SCLK 7
#define DISPLAY_PIN_SS 4
#define DISPLAY_SPI_vect SPIC_INT_vect

#define _SCLK_bm G4_PIN(DISPLAY_PIN_SCLK)
#define _SOUT_bm G4_PIN(DISPLAY_PIN_SOUT)
#define _SS_bm G4_PIN(DISPLAY_PIN_SS)

static comm_driver_t *comm;
static mempool_t *mempool;
static TaskHandle_t display_task_handle;

static void tx_begin(void);
static void tx_end(void);
static portTASK_FUNCTION(display_tx, params);

inline void display_init(void) {
	//SS will fuck you over hard per xmegaA, pp226.
	DISPLAY_PORT.DIRSET = _SCLK_bm | _SOUT_bm | _SS_bm;
	DISPLAY_PORT.OUTSET = _SCLK_bm | _SOUT_bm | _SS_bm;


	//32MhZ, DIV4 = 8, CLK2X => 16Mhz. = 1/16uS per bit. *8 => 1-2uS break to latch.
	DISPLAY_SPI.CTRL = SPI_ENABLE_bm | SPI_MASTER_bm | /*SPI_CLK2X_bm |*/ SPI_PRESCALER_DIV128_gc;
	DISPLAY_SPI.INTCTRL = SPI_INTLVL_LO_gc;

	//note passing 0 for rxbuff size - RX is not used!
	//display_uart_driver = uart_init(&DISPLAY_SPI.DATA, tx_begin, tx_end, 0);

	mempool = init_mempool(DISPLAY_PKT_MAX_SIZE + sizeof(comm_frame_t), 2);

	comm_dev_t *commdev;
	commdev = serialcomm_init(&DISPLAY_SPI.DATA, tx_begin, tx_end, 1 /*dummy address*/);
	comm = comm_init( commdev, 1 /*dummy address*/, DISPLAY_PKT_MAX_SIZE, mempool, NULL );

	xTaskCreate(display_tx, "display", 128, NULL, tskIDLE_PRIORITY + 2, &display_task_handle);
}

static void tx_begin(void) {
	DISPLAY_SPI.INTCTRL = SPI_INTLVL_LO_gc;
	DISPLAY_PORT.OUTCLR = _SS_bm;
	//because with SPI the ISR only triggers whena  byte finishes transferring
	serialcomm_tx_isr(comm);
}

static void tx_end(void) {
	DISPLAY_SPI.INTCTRL = SPI_INTLVL_OFF_gc;
	DISPLAY_PORT.OUTSET = _SS_bm;
}

static portTASK_FUNCTION(display_tx, params) {
	comm_tx(comm);
	while (1) {
		if (xTaskNotifyWait(0, 0, NULL, portMAX_DELAY) == pdTRUE)
			comm_tx(comm);
	}
}

inline void display_send(const uint8_t cmd, const uint8_t size, uint8_t *data) {
	comm_frame_t *frame;
	display_pkt *pkt;

	frame = mempool_alloc(mempool);
	frame->daddr = 1 /*dummy*/;
	frame->size = sizeof(*pkt) + size;

	pkt = (display_pkt *)frame->data;

	pkt->cmd = cmd;
	pkt->size = size;

	memcpy(pkt->data, data, pkt->size);

	comm_send(comm, mempool_getref(frame));
	mempool_putref(frame);

	xTaskNotify(display_task_handle, 0, eNoAction);
}

inline void display_write(char *str) {
	display_send(0, strlen(str) + 1, (uint8_t *)str);
}

ISR(DISPLAY_SPI_vect) {
	serialcomm_tx_isr(comm);
}
