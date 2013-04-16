#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#ifndef SCHEDULER_H
#define SCHEDULER_H

// set prescaler to 1
#define SCHEDULER_CONTROL_REGISTER TCC0.CTRLA
#define SCHEDULER_PRESCALER_BITS TC_CLKSEL_DIV1_gc

#define SCHEDULER_REGISTER TCC0.CNT

#define SCHEDULER_PRESCALER 1

#define SCHEDULER_INTERRUPT_REGISTER TCC0.INTCTRLB
#define SCHEDULER_INTERRUPT_ENABLE_BITS TC_CCAINTLVL_MED_gc

//to change shit to a compare match...
//#define SCHEDULER_INTERRUPT_ENABLE_BITS _BV(OCIE2A);
//#define SCHEDULER_INTERRUPT_VECTOR TIMER2_COMPA_vect
//#define SCHEDULER_HZ (F_CPU/128)


#define SCHEDULER_INTERRUPT_VECTOR TCC0_CCA_vect

//we need the scheduler to use interrupts,
//so rather than counting the scheduler ticks, 
//we count the overflow ticks, which acts a a prescaler.
#define SCHEDULER_HZ (((F_CPU/SCHEDULER_PRESCALER)/256))
//TEMP - IFDK why it's running 2x as fast
//#define SCHEDULER_HZ 131071
//#define SCHEDULER_HZ 62652
// the number of microseconds per tick.
#define SCHEDULER_TICK_US (1000000/SCHEDULER_HZ)


#define SCHEDULER_RUN ISR(SCHEDULER_INTERRUPT_VECTOR)

//used as argument 3 to scheduler_register 
//to make the task run indefinitely.
#define SCHEDULER_RUN_UNLIMITED 0

typedef uint32_t task_freq_t;
typedef uint32_t task_ticks_t;
typedef uint8_t task_lifetime_t;

typedef struct {
	void (* task)(void);

	//number of ticks between runs.
	task_freq_t freq;

	//number of ticks since last run.
	task_ticks_t ticks;

	//number of times to run.
	//SCHEDULER_RUN_UNLIMITED = infinite.
	task_lifetime_t lifetime;

} scheduler_task;

void scheduler_init(void);

void scheduler_run(void);

void scheduler_register(void (*)(void), task_freq_t, task_lifetime_t );

void scheduler_unregister(void (*)(void));

struct task_node_st {
	scheduler_task * task;
	struct task_node_st * next;
};

typedef struct task_node_st task_node;


#endif
