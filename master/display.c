#include <avr/io.h>
#include <stdint.h>

#include <util.h>
#include <uart.h>

#include "display.h"

#define DISPLAY_SPI SPIC
#define DISPLAY_PORT PORTC
#define DISPLAY_PIN_SOUT 5
#define DISPLAY_PIN_SCLK 7
#define DISPLAY_PIN_SS 4

#define _SCLK_bm G4_PIN(DISPLAY_PIN_SCLK)
#define _SOUT_bm G4_PIN(DISPLAY_PIN_SOUT)
#define _SS_bm G4_PIN(DISPLAY_PIN_SS)

static uart_driver_t * display_uart_driver;

static void tx_begin(void);
static void tx_end(void);

inline void display_init(void) {
	//SS will fuck you over hard per xmegaA, pp226.
	DISPLAY_PORT.DIRSET = _SCLK_bm | _SOUT_bm | _SS_bm;
	DISPLAY_PORT.OUTSET = _SCLK_bm | _SOUT_bm | _SS_bm;


	//32MhZ, DIV4 = 8, CLK2X => 16Mhz. = 1/16uS per bit. *8 => 1-2uS break to latch.
	DISPLAY_SPI.CTRL = SPI_ENABLE_bm | SPI_MASTER_bm | SPI_CLK2X_bm | SPI_PRESCALER_DIV4_gc;
	DISPLAY_SPI.INTCTRL = SPI_INTLVL_LO_gc;

	display_uart_driver = uart_init(&DISPLAY_SPI.DATA, tx_begin, tx_end, 2);
}

static void tx_begin(void) {
	DISPLAY_SPI.INTCTRL = SPI_INTLVL_LO_gc;
	DISPLAY_PORT.OUTCLR = _SS_bm;
}

static void tx_end(void) {
	DISPLAY_SPI.INTCTRL = SPI_INTLVL_OFF_gc;
	DISPLAY_PORT.OUTSET = _SS_bm;
}

inline void display_send(const uint8_t cmd, uint8_t * data, const uint8_t size) {
	uart_tx(display_uart_driver, cmd, size, data);
}

