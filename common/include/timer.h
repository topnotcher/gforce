#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#ifndef TIMER_H
#define TIMER_H
#define TIMER_INTERRUPT_REGISTER RTC.INTCTRL
#define TIMER_INTERRUPT_ENABLE_BITS RTC_COMPINTLVL_HI_gc
#define TIMER_INTERRUPT_VECTOR RTC_COMP_vect

#define TIMER_HZ 1024
// the number of microseconds per tick.
#define TIMER_TICK_US (1000000/TIMER_HZ)

#define TIMER_RUN ISR(TIMER_INTERRUPT_VECTOR)

//used as argument 3 to timer_register 
//to make the task run indefinitely.
#define TIMER_RUN_UNLIMITED 0

typedef uint16_t timer_ticks_t;
typedef uint8_t timer_lifetime_t;

void init_timers(void);
void add_timer(void (*)(void), timer_ticks_t, timer_lifetime_t );
void del_timer(void (*)(void));

#endif
