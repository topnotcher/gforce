#include <stdlib.h>
#include <stddef.h>

#include <g4config.h>
#include <mpc.h>
#include <irrx.h>

#include "config.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>


#define MAX_PKT_SIZE 15
#define RX_BUF_SIZE MAX_PKT_SIZE

//when the packet timer reaches this level, set the state to process.
// 1/IR_BAUD*mil = uS per bit. Timeout after 11 bits. (too generous)
//this is the number of timer ticks per IR bits.
#define RX_TIMEOUT_TIME (task_freq_t)(1 / IR_BAUD * 1000000 / TIMER_TICK_US)
#define RX_TIMEOUT_TICKS 11 //time out after this many bit widths with no data (too low?)


typedef struct {
	// received packet buffer
	uint8_t buf[RX_BUF_SIZE];

	// maximum size for the command being received
	// This could be changed based on the command, which is the first byte.
	uint8_t max_bytes;

	// number of bytes in the received packet buffer
	uint8_t bytes; 

	// running CRC of buffer.
	uint8_t crc;

	enum {
		// not currently receiving a packet
		RX_STATE_IDLE,

		// receiving a packet. We enter this state when we receive a start byte.
		RX_STATE_RECEIVE,
	} state;
} rx_state_t;


static uint8_t process_rx_byte(rx_state_t *, uint8_t);
static void ir_rx_task(void *);
static void process_rx_data(rx_state_t *);

static QueueHandle_t g_rx_queue;

// TODO: this is not implemented on SAML21 yet
void __attribute__((weak)) irrx_hw_init(QueueHandle_t _) {}

void irrx_init(void) {

	QueueHandle_t rx_queue = xQueueCreate(24, sizeof(uint16_t));
	xTaskCreate(ir_rx_task, "ir-rx", 128, rx_queue, tskIDLE_PRIORITY + 3, (TaskHandle_t*)NULL);

	irrx_hw_init(rx_queue);
}

static void ir_rx_task(void *p_queue) {
	QueueHandle_t rx_queue = p_queue;
	g_rx_queue = rx_queue;

	rx_state_t rx_state;
	uint16_t data;

	rx_state.state = RX_STATE_IDLE;

	while (1) {
		// TODO: use delay to set inter-byte timeout.
		if (xQueueReceive(rx_queue, &data, portMAX_DELAY)) {

			// bit 8 clear always indicates start of packet.
			if (data == 0x0FF) {
				if (rx_state.state != RX_STATE_IDLE) {
					process_rx_data(&rx_state);
				}

				rx_state.max_bytes = MAX_PKT_SIZE;
				rx_state.bytes = 0;
				rx_state.state = RX_STATE_RECEIVE;
				rx_state.crc = IR_CRC_SHIFT;

			} else if (rx_state.state == RX_STATE_RECEIVE) {
				if (process_rx_byte(&rx_state, data & 0xFF)) {
					process_rx_data(&rx_state);
				}
			}

		// inter-byte timeout. If we can process the buffered data, lets do so.
		} else {
			process_rx_data(&rx_state);
		}
	}
}

static void process_rx_data(rx_state_t *rx_state) {
	// The last byte should always be the CRC of the whole packet
	if (rx_state->crc == rx_state->buf[rx_state->bytes - 1])
		mpc_send(MPC_ADDR_MASTER, MPC_CMD_IR_RX, rx_state->bytes, rx_state->buf);

	rx_state->state = RX_STATE_IDLE;
}

static uint8_t process_rx_byte(rx_state_t *rx_state, uint8_t data) {

	if (rx_state->bytes == 0) {

		/**
		 * TODO: This is a tad ghetto
		 */
		rx_state->max_bytes = (data == 0x38) ? MAX_PKT_SIZE : 4;
		rx_state->buf[0] = data;
		rx_state->bytes = 1;

		return 0;
	} else {

		// The last byte we put in he buffer was the header CRC
		if (rx_state->bytes == offsetof(ir_hdr_t, crc) + 1) {

			// CRC does not match. Set the receiver to idle and wait for the
			// next start byte. For some packets we will check this twice: the
			// CRC should always match the last byte in the packet. Long
			// packets contain two CRC bytes: one for just the header, and one
			// for the entire packet.
			if (rx_state->crc != rx_state->buf[rx_state->bytes - 1])
				rx_state->state = RX_STATE_IDLE;

		} else {
			g4_ir_crc(&rx_state->crc, rx_state->buf[rx_state->bytes - 1], IR_CRC_POLY);
		}

		rx_state->buf[rx_state->bytes] = data;

		if (++rx_state->bytes == rx_state->max_bytes)
			return 1;
		else
			return 0;
	}
}

void ir_rx_simulate(const uint8_t size, const uint8_t *const data) {
	if (g_rx_queue) {
		uint16_t rx_byte;

		rx_byte = 0xFF;
		xQueueSend(g_rx_queue, &rx_byte, portMAX_DELAY);

		for (uint8_t i = 0; i < size; ++i) {
			rx_byte = *(data + i) | ((i > 2) ? (1 << 8) : 0);
			xQueueSend(g_rx_queue, &rx_byte, 0);
		}
	}
}

void g4_ir_crc(uint8_t *const shift, uint8_t byte, const uint8_t poly) {
	uint8_t fb;

	for (uint8_t i = 0; i < 8; i++) {
		fb = (*shift ^ byte) & 0x01;

		if (fb)
			*shift = 0x80 | (((*shift) ^ poly) >> 1);
		else
			*shift >>= 1;
		byte >>= 1;
	}
}

void ir_rx_recive(uint16_t data) {
	xQueueSend(g_rx_queue, &data, portMAX_DELAY);
}
