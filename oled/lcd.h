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
#define LCD_CLK_DELAY() /*__asm__ __volatile__ ("nop")*/ _delay_us(25)

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


void lcd_init(void);
void lcd_cmd(char);
void lcd_write(char);

void lcd_out_raw(char);
void lcd_out(char, lcd_data_type);
void lcd_out_upper(char);
void lcd_out_lower(char);

void lcd_clk(void);
void lcd_bl_on(void);
void lcd_bl_off(void);
void lcd_putstring(char *);

void lcd_clear(void);

void lcd_tick(void);

bool lcd_busy(void);

void lcd_toggle_read(bool);

#endif
