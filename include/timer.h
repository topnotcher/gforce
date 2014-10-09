#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#ifndef TIMER_H
#define TIMER_H
#define TIMER_INTERRUPT_REGISTER RTC.INTCTRL
#define TIMER_INTERRUPT_ENABLE_BITS RTC_COMPINTLVL_HI_gc

//to change shit to a compare match...
//#define TIMER_INTERRUPT_ENABLE_BITS _BV(OCIE2A);
//#define TIMER_INTERRUPT_VECTOR TIMER2_COMPA_vect
//#define TIMER_HZ (F_CPU/128)


#define TIMER_INTERRUPT_VECTOR RTC_COMP_vect

//we need the timer to use interrupts,
//so rather than counting the timer ticks, 
//we count the overflow ticks, which acts a a prescaler.
//this is every 256 CPU instructions? seems a bit fuckin steeep. WE SHALL GO WITH IT

#define TIMER_HZ 1000
//TEMP - IFDK why it's running 2x as fast
//#define TIMER_HZ 131071
//#define TIMER_HZ 62652
// the number of microseconds per tick.
#define TIMER_TICK_US (1000000/TIMER_HZ)

#define TIMER_RUN ISR(TIMER_INTERRUPT_VECTOR)

//used as argument 3 to timer_register 
//to make the task run indefinitely.
#define TIMER_RUN_UNLIMITED 0

typedef uint16_t timer_freq_t;
typedef uint16_t timer_ticks_t;
typedef uint8_t timer_lifetime_t;

void init_timers(void);
void add_timer(void (*)(void), timer_freq_t, timer_lifetime_t );
void del_timer(void (*)(void));

#endif
