#include <string.h>
#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

#include "lcd.h"
#include "ports.h"
#include "scheduler.h"

lcd_queue_t lcd_queue;

inline void lcd_init() {

	LCD_PORT.DIRSET |= LCD_CS | LCD_SDI | LCD_SCL;
	LCD_PORT.OUTSET |= LCD_CS;


	//low by default.
//	PIN_LOW(LCD_PORT,LCD_E);
//	PIN_LOW(LCD_PORT, LCD_RS);
//	PIN_LOW(LCD_PORT,LCD_RW);

	//off by default
//	lcd_bl_off();

	//15?
	_delay_ms(5);

	//wake
	//
//	lcd_out_upper(0x30);
//	_delay_ms(30);

//	lcd_out_upper(0x30);
///	_delay_ms(10);
//
//	lcd_out_upper(0x30);
///	_delay_ms(10);

	//function set, 4-bit
//	lcd_out_upper(0x20);

//	_delay_ms(100);

	//function set, 4-bit, 2-line 
//	lcd_out_raw(0x28);

//	_delay_us(100);
	
	//display on??? 	
//	lcd_out_raw(0x08); 

//	_delay_us(100);
	
	//clear
//	lcd_out_raw(0x01);
	
//	_delay_ms(4);
	
	//entry mode set
//	lcd_out_raw(0x06);
	
//	_delay_us(100);
//	
	//display on???
	//0x0E works. 0x0F just adds blinking cursor
	lcd_cmd(0x0F);	
///	lcd_out_raw(0x0e);



//	_delay_us(100);

	memset(&lcd_queue, 0, sizeof lcd_queue);

}

/**
 * completely raw output
 */
void lcd_out_raw(char data) {
/*	lcd_out_upper(data);
	lcd_out_lower(data);
	=*/
	
	uint8_t i = 7;
	while ( 1 ) {
		if ( (data>>i)&1 ) 
			LCD_PORT.OUTSET |= LCD_SDI;
		else
			LCD_PORT.OUTCLR |= LCD_SDI;

		lcd_clk();

		if ( i-- == 0 ) break;
	}
}

void lcd_out(char data, lcd_data_type type) {

	if ( lcd_queue.read == lcd_queue.write )
		scheduler_register(&lcd_tick, LCD_SCHEDULER_FREQ ,SCHEDULER_RUN_UNLIMITED);


	lcd_queue.data[lcd_queue.write].data = data;
	lcd_queue.data[lcd_queue.write].type = type;

	lcd_queue.write = (lcd_queue.write == LCD_QUEUE_MAX-1) ? 0 : lcd_queue.write+1;
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

inline void lcd_clk() {
	LCD_PORT.OUTSET |= LCD_SCL;
	_delay_us(1);
	LCD_PORT.OUTCLR |= LCD_SCL;
}

//in theory, this should speed things up a bit when we
//check the busy flag multiple times.
inline void lcd_toggle_read(const bool read) {
	return false;
	//by default, we're in write mode.
/*	static bool is_read = false;

	if ( read && !is_read ) {
		is_read = true;

		//set data ports to read
		LCD_DDR = 0XFF&~(_BV(LCD_DB_4) | _BV(LCD_DB_5) | _BV(LCD_DB_6) | _BV(LCD_DB_7));

		// enable pullup and read.
		LCD_PORT |= _BV(LCD_DB_4) | _BV(LCD_DB_5) | _BV(LCD_DB_6) | _BV(LCD_DB_7) | _BV(LCD_RW);

		PIN_LOW(LCD_PORT,LCD_RS);

	} else if ( !read && is_read ) {
		is_read = false;

		//set data ports to read
		LCD_DDR = 0XFF;

		
		PIN_LOW(LCD_PORT,LCD_RW);

	}
*/
}
bool lcd_busy() {
	return false;
/*	lcd_toggle_read(true);

	lcd_clk();
	uint8_t val = (LCD_PIN & _BV(LCD_DB_7));
	lcd_clk();

	return !(val == 0);*/
}

void lcd_tick() {
	// nothing to do here!
	if ( lcd_queue.read == (lcd_queue.write-1) ) {
		scheduler_unregister(&lcd_tick);
	}

	LCD_PORT.OUTCLR |= LCD_CS;

	//first we output RS
	if ( lcd_queue.data[lcd_queue.read].type == LCD_TYPE_CMD ) {
		LCD_PORT.OUTSET |= LCD_SDI;
		lcd_clk();
	} 
	else {
		LCD_PORT.OUTCLR |= LCD_SDI;
		lcd_clk();
	}

	//now RW 
	LCD_PORT.OUTCLR |= LCD_SDI;
	lcd_clk();
		
	
	lcd_out_raw(lcd_queue.data[lcd_queue.read].data);

	//EOT.
	LCD_PORT.OUTSET |= LCD_CS;

	lcd_queue.read = (lcd_queue.read == LCD_QUEUE_MAX-1) ? 0 : lcd_queue.read+1;
}

inline void lcd_bl_off() {
//	LCD_PORT |= _BV(LCD_BL);
}

inline void lcd_bl_on() {
//	LCD_PORT &= ~_BV(LCD_BL);
}

inline void lcd_clear() {
	lcd_cmd(0x01);
}
