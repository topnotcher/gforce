#include <avr/io.h>
#include <avr/interrupt.h>

#ifndef TIMER_H
#define TIMER_H


#define TIMER_INTERRUPT_REGISTER RTC.INTCTRL
#define TIMER_INTERRUPT_ENABLE_BITS RTC_OVFINTLVL_HI_gc
#define TIMER_INTERRUPT_VECTOR RTC_OVF_vect

#define TIMER_RUN ISR(TIMER_INTERRUPT_VECTOR)

void init_timers(void);

#endif
