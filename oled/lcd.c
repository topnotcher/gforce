#include <string.h>
#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

#include <util.h>
#include "lcd.h"

#define PIN_LOW(PORT,PIN_bm) PORT.OUTCLR = PIN_bm
#define PIN_HIGH(PORT,PIN_bm) PORT.OUTSET = PIN_bm

#define _E_bm  G4_PIN(LCD_E)
#define _RS_bm G4_PIN(LCD_RS)
#define _RW_bm G4_PIN(LCD_RW)

volatile lcd_queue_t lcd_queue;

inline void lcd_init(void) {
	LCD_CONTROL_PORT.DIRSET = _E_bm | _RS_bm | _RW_bm; 
	LCD_DATA_PORT.DIRSET = 0xFF;

	//low by default.
	PIN_LOW(LCD_CONTROL_PORT, _E_bm );
	PIN_LOW(LCD_CONTROL_PORT, _RS_bm);
	PIN_LOW(LCD_CONTROL_PORT, _RW_bm);

	//off by default
//	lcd_bl_off();

	//15?
	_delay_ms(100);

	//wake
	//
	lcd_out_upper(0x30);
	_delay_ms(30);

	lcd_out_upper(0x30);
	_delay_ms(10);

	lcd_out_upper(0x30);
	_delay_ms(10);

	//function set, 4-bit
	lcd_out_upper(0x20);

	_delay_ms(100);

	//function set, 4-bit, 2-line 
	lcd_out_raw(0x28);

	_delay_us(100);
	
	//display on??? 	
	lcd_out_raw(0x08); 

	_delay_us(100);
	
	//clear
	lcd_out_raw(0x01);
	
	_delay_ms(4);
	
	//entry mode set
	lcd_out_raw(0x06);
	
	_delay_us(100);
	
	//display on???
	//0x0E works. 0x0F just adds blinking cursor
	//lcd_cmd(0x0F);	
	lcd_out_raw(0x0e);
	 
	_delay_us(100);

	//memset((void *)&lcd_queue, 0, sizeof lcd_queue);

}

/**
 * completely raw output
 */
void lcd_out_raw(char data) {
	lcd_out_upper(data);
	lcd_out_lower(data);
}

void lcd_out(char data, lcd_data_type type) {

//	if ( lcd_queue.read == lcd_queue.write )
//		scheduler_register(&lcd_tick, LCD_SCHEDULER_FREQ ,SCHEDULER_RUN_UNLIMITED);


	lcd_queue.data[lcd_queue.write].data = data;
	lcd_queue.data[lcd_queue.write].type = type;

	lcd_queue.write = (lcd_queue.write == LCD_QUEUE_MAX-1) ? 0 : lcd_queue.write+1;
}

inline void lcd_out_upper(char data) {

	//set the upper 4 bits
	LCD_DATA_PORT.OUT = data;
//	PIN_SET(LCD_PORT, LCD_DB_4, data&0b00010000);
//	PIN_SET(LCD_PORT, LCD_DB_5, data&0b00100000);
//	PIN_SET(LCD_PORT, LCD_DB_6, data&0b01000000);
//	PIN_SET(LCD_PORT, LCD_DB_7, data&0b10000000);

	lcd_clk();
}

inline void lcd_out_lower(char data) {

//	//set the lower 4 bits
	LCD_DATA_PORT.OUT = data<<4; 
//	PIN_SET(LCD_PORT, LCD_DB_4, data&0b00000001);
///	PIN_SET(LCD_PORT, LCD_DB_5, data&0b00000010);
//	PIN_SET(LCD_PORT, LCD_DB_6, data&0b00000100);
//	PIN_SET(LCD_PORT, LCD_DB_7, data&0b00001000);

	lcd_clk();
}

inline void lcd_cmd(char data) {
	lcd_out(data,LCD_TYPE_CMD);
}

void lcd_putstring(char * string) {
	
	while ( *string != '\0' ) {
		lcd_write( *string );
		string++;
	}
}

inline void lcd_write(char data) {
	lcd_out(data, LCD_TYPE_TEXT);
}

inline void lcd_clk(void) {
	PIN_HIGH(LCD_CONTROL_PORT, _E_bm);
	LCD_CLK_DELAY();
	PIN_LOW(LCD_CONTROL_PORT, _E_bm);
}

//in theory, this should speed things up a bit when we
//check the busy flag multiple times.
inline void lcd_toggle_read(const bool read) {

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
bool lcd_busy(void) {

	lcd_toggle_read(true);

	lcd_clk();
	uint8_t val = (LCD_DATA_PORT.IN & G4_PIN(7));
	lcd_clk();

	return !(val == 0);
}

void lcd_tick(void) {
	// nothing to do here!
//	if ( lcd_queue.read == (lcd_queue.write-1) ) {
//		scheduler_unregister(&lcd_tick);
//	}

	if (lcd_busy()) 
		return;

	lcd_toggle_read(false);

	if ( lcd_queue.data[lcd_queue.read].type == LCD_TYPE_CMD )
		PIN_LOW(LCD_CONTROL_PORT, _RS_bm);
	else
		PIN_HIGH(LCD_CONTROL_PORT,_RS_bm);
	
	lcd_out_raw(lcd_queue.data[lcd_queue.read].data);
	lcd_queue.read = (lcd_queue.read == LCD_QUEUE_MAX-1) ? 0 : lcd_queue.read+1;
}

inline void lcd_clear(void) {
	lcd_cmd(0x01);
}
