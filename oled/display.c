#include <string.h>
#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

#include <util.h>
#include "display.h"

#define PIN_LOW(PORT,PIN_bm) PORT.OUTCLR = PIN_bm
#define PIN_HIGH(PORT,PIN_bm) PORT.OUTSET = PIN_bm

#define _E_bm  G4_PIN(LCD_E)
#define _RS_bm G4_PIN(LCD_RS)
#define _RW_bm G4_PIN(LCD_RW)

volatile lcd_queue_t lcd_queue;

static inline void lcd_toggle_read(const bool read);
static inline void lcd_clk(void);
static inline void display_out_raw(char data);
static inline bool lcd_busy(void);


inline void display_init(void) {
	LCD_CONTROL_PORT.DIRSET = _E_bm | _RS_bm | _RW_bm; 
	LCD_DATA_PORT.DIRSET = 0xFF;

	//low by default.
	PIN_LOW(LCD_CONTROL_PORT, _E_bm );
	PIN_LOW(LCD_CONTROL_PORT, _RS_bm);
	PIN_LOW(LCD_CONTROL_PORT, _RW_bm);

	//15
	_delay_ms(100);
	
	//wake
	display_out_raw(0x30);
	_delay_ms(30);
	display_out_raw(0x30);
	_delay_ms(10);
	display_out_raw(0x30);
	_delay_ms(10);
	
	//set to 8bit/2 line (Func set)
	display_out_raw(0x38); //try 3c?
	
	_delay_us(100);
	
	//display on??? 	
	display_out_raw(0x08); //0x0E works. 0x0F just adds blinking cursor

	_delay_us(100);
	
	//clear
	display_out_raw(0x01);
	//home
	display_out_raw(0x02);
	
	
	_delay_ms(4);
	
	//entry mode set
	display_out_raw(0x06);
	
	_delay_us(100);
	
	//display on??? 	
	display_out_raw(0x0F); //0x0E works. 0x0F just adds blinking cursor
	
	_delay_us(100);

}

/**
 * completely raw output
 */
static inline void display_out_raw(char data) {
	LCD_DATA_PORT.OUT = data;
	lcd_clk();
}

void display_out(char data, lcd_data_type type) {

//	if ( lcd_queue.read == lcd_queue.write )
//		scheduler_register(&display_tick, LCD_SCHEDULER_FREQ ,SCHEDULER_RUN_UNLIMITED);


	lcd_queue.data[lcd_queue.write].data = data;
	lcd_queue.data[lcd_queue.write].type = type;

	lcd_queue.write = (lcd_queue.write == LCD_QUEUE_MAX-1) ? 0 : lcd_queue.write+1;
}

void display_putstring(char * string) {
	
	while ( *string != '\0' ) {
		display_write( *string );
		string++;
	}
}

static inline void lcd_clk(void) {
	PIN_HIGH(LCD_CONTROL_PORT, _E_bm);
	LCD_CLK_DELAY();
	PIN_LOW(LCD_CONTROL_PORT, _E_bm);
}

//in theory, this should speed things up a bit when we
//check the busy flag multiple times.
static inline void lcd_toggle_read(const bool read) {

	//by default, we're in write mode.
	static bool is_read = false;

	if ( read && !is_read ) {
		is_read = true;

		//set data ports to read
		LCD_DATA_PORT.DIRCLR = 0xFF;
		//= 0XFF&~(_BV(LCD_DB_4) | _BV(LCD_DB_5) | _BV(LCD_DB_6) | _BV(LCD_DB_7));

		// enable pullup and read.
		//LCD_PORT |= _BV(LCD_DB_4) | _BV(LCD_DB_5) | _BV(LCD_DB_6) | _BV(LCD_DB_7) | _BV(LCD_RW);
		PIN_HIGH(LCD_CONTROL_PORT, _RW_bm);

		PIN_LOW(LCD_CONTROL_PORT, _RS_bm);

	} else if ( !read && is_read ) {
		is_read = false;

		LCD_DATA_PORT.DIRSET = 0xFF;

		PIN_LOW(LCD_CONTROL_PORT, _RW_bm);

	}

}
static inline bool lcd_busy(void) {

	lcd_toggle_read(true);

	lcd_clk();
	uint8_t val = (LCD_DATA_PORT.IN & G4_PIN(7));
	lcd_clk();

	return !(val == 0);
}

void display_tick(void) {
	// nothing to do here!
//	if ( lcd_queue.read == (lcd_queue.write-1) ) {
//		scheduler_unregister(&display_tick);
//	}

	if ( lcd_queue.read == lcd_queue.write )
		return;

	if (lcd_busy()) 
		return;

	lcd_toggle_read(false);

	if ( lcd_queue.data[lcd_queue.read].type == LCD_TYPE_CMD )
		PIN_LOW(LCD_CONTROL_PORT, _RS_bm);
	else
		PIN_HIGH(LCD_CONTROL_PORT,_RS_bm);
	
	display_out_raw(lcd_queue.data[lcd_queue.read].data);
	lcd_queue.read = (lcd_queue.read == LCD_QUEUE_MAX-1) ? 0 : lcd_queue.read+1;
}
