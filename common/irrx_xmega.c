#include <avr/io.h>
#include <avr/interrupt.h>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <drivers/xmega/util.h>
#include <drivers/xmega/uart.h>

#include <g4config.h>
#include <irrx.h>

#include "config.h"

#define _RXPIN_bm G4_PIN(IRRX_USART_RXPIN)

static void irrx_usart_rx_vect(void *);

void irrx_hw_init(QueueHandle_t rx_queue) {
	int8_t uart_index = uart_get_index(&IRRX_USART);

	IRRX_USART_PORT.DIRCLR = _RXPIN_bm;

	/** These values are from G4CONFIG, NOT board config **/
	IRRX_USART.BAUDCTRLA = (uint8_t)(IR_USART_BSEL & 0x00FF);
	IRRX_USART.BAUDCTRLB = (IR_USART_BSCALE << USART_BSCALE_gp) | (uint8_t)((IR_USART_BSEL >> 8) & 0x0F);

	uart_register_handler(uart_index, UART_RXC_VEC, irrx_usart_rx_vect, rx_queue);

	IRRX_USART.CTRLA |= USART_RXCINTLVL_MED_gc;
	IRRX_USART.CTRLB |= USART_RXEN_bm;

	//NOTE : removed 1<<USART_SBMODE_bp Why does LW use 2 stop bits?
	IRRX_USART.CTRLC = USART_PMODE_DISABLED_gc | USART_CHSIZE_9BIT_gc;
}

static void irrx_usart_rx_vect(void *param) {
	QueueHandle_t rx_queue = param;
	uint16_t value = ((IRRX_USART.STATUS & USART_RXB8_bm) ? 0x100 : 0x00) |  IRRX_USART.DATA;

	xQueueSendFromISR(rx_queue, &value, NULL);
}
