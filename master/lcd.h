#include <stdint.h>
#include <stdbool.h>

#ifndef LCD_H
#define LCD_H

/**
 * Definitions for LCD
 */
#define LCD_PORT PORTC
#define LCD_SPI  SPIC

#define LCD_CS   PIN4_bm
#define LCD_SDI PIN5_bm
#define LCD_SDO PIN6_bm
#define LCD_SCL PIN7_bm


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
void lcd_putstring(char *);

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
