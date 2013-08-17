#include <stdint.h>
#include <stdbool.h>

#ifndef LCD_H
#define LCD_H

/**
 * Definitions for LCD
 */
#define LCD_CONTROL_PORT PORTB

//DB0->7 = PORTC0-7
#define LCD_DATA_PORT PORTC

#define LCD_E  2
#define LCD_RW 1
#define LCD_RS 0

#define LCD_DB_OFFSET 3

//this should be >= 300ns.
//I lowered it because it worked. shrug.
#define LCD_CLK_DELAY() _delay_us(1) /*__asm__ __volatile__ ("nop; nop; nop; nop; nop; nop; nop; nop; nop;")*/
/*_delay_us(25)*/

// I pulled 10 out of my ass :).
// This number should be small enoguh to avoid lag
// Other than that, making it larger will free the CPU up for other shit.
#define LCD_SCHEDULER_FREQ 10

typedef uint8_t lcd_data_type;
#define LCD_TYPE_CMD 0
#define LCD_TYPE_TEXT 1

typedef struct {
	 	lcd_data_type type;
		char data;
} lcd_data;

#define LCD_QUEUE_MAX 50
typedef struct {
	uint8_t read;
	uint8_t write;
	lcd_data data[LCD_QUEUE_MAX];
} lcd_queue_t;


void display_init(void);
void display_out(char, lcd_data_type);
void display_putstring(char *);
void display_tick(void);

#define display_cmd(/*char*/ data) display_out(data,LCD_TYPE_CMD)
#define display_clear() display_cmd(0x01); display_cmd(2)
#define display_write(/*char*/ data) display_out(data, LCD_TYPE_TEXT) 

#endif
