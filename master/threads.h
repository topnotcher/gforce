#ifndef THREADS_H
#define THREADS_H

#define THREADS_CONTEXT_SIZE 38
#define THREADS_STACK_SIZE 70

#ifndef NUM_THREADS
#define NUM_THREADS 5
#endif

#include <queue.h>

typedef enum {
	THREAD_PRIORITY_HIGH = 0,
	THREAD_PRIORITY_MEDIUM = 1,
	THREAD_PRIORITY_LOW = 2,
	THREAD_PRIORITY_IDLE = 3,
} thread_priority_t;

typedef struct {
	uint8_t pid;
	void * stack;
	const char * name;
	enum {
		THREAD_RUNNING,
		THREAD_RUNNABLE,
		THREAD_SUSPENDED,
	} state;

	thread_priority_t priority;
} tcb_t;

typedef struct {
	uint8_t num;
	tcb_t * tcb;
	void * top_of_stack;
	tcb_t list[NUM_THREADS];	
	queue_t *run_queue[THREAD_PRIORITY_IDLE + 1];
} threads_t;

extern threads_t threads;

/**
 * Sets the stack pointer to the top of the "main" thread of coroutines.
 * (since main is never "started" as a task...)
 * 16 is subtracted as a buffer - enough to get the first thread launched.
 */
#define threads_init_stack()\
	asm volatile(\
		"in %A0, __SP_L__\n"\
		"in %B0, __SP_H__\n"\
		"sbci %A0, 16\n"\
		: "=e" (threads.top_of_stack) : :\
	)\



void threads_init(void);
void thread_yield(void) __attribute__((naked));
void thread_suspend(uint8_t *) __attribute__((naked));
void threads_start(void) __attribute__((naked));
uint8_t thread_create(const char * name, void (*task)(void), uint16_t stack_size, thread_priority_t priority);
uint8_t thread_pid(void);
void thread_wake_and_schedule(uint8_t tid) __attribute__((naked));
void thread_wake(uint8_t);

#define thread_context_in()                                \
	asm volatile(\
                    "out    __SP_L__, %A0            \n\t"    \
                    "out    __SP_H__, %B0            \n\t"    \
                    "pop    r31                      \n\t"    \
                    "pop    r30                      \n\t"    \
                    "pop    r29                      \n\t"    \
                    "pop    r28                      \n\t"    \
                    "pop    r27                      \n\t"    \
                    "pop    r26                      \n\t"    \
                    "pop    r25                      \n\t"    \
                    "pop    r24                      \n\t"    \
                    "pop    r23                      \n\t"    \
                    "pop    r22                      \n\t"    \
                    "pop    r21                      \n\t"    \
                    "pop    r20                      \n\t"    \
                    "pop    r19                      \n\t"    \
                    "pop    r18                      \n\t"    \
                    "pop    r17                      \n\t"    \
                    "pop    r16                      \n\t"    \
                    "pop    r15                      \n\t"    \
                    "pop    r14                      \n\t"    \
                    "pop    r13                      \n\t"    \
                    "pop    r12                      \n\t"    \
                    "pop    r11                      \n\t"    \
                    "pop    r10                      \n\t"    \
                    "pop    r9                       \n\t"    \
                    "pop    r8                       \n\t"    \
                    "pop    r7                       \n\t"    \
                    "pop    r6                       \n\t"    \
                    "pop    r5                       \n\t"    \
                    "pop    r4                       \n\t"    \
                    "pop    r3                       \n\t"    \
                    "pop    r2                       \n\t"    \
                    "pop    r1                       \n\t"    \
                    "pop    r0                       \n\t"    \
                    "out    __SREG__, r0             \n\t"    \
                    "pop    r0                       \n\t"    \
					: : "e" (threads.tcb->stack) : \
                )
#define thread_context_out()                                   \
    asm volatile (  "push    r0                     \n\t"    \
                    "in      r0, __SREG__           \n\t"    \
                    "cli                            \n\t"    \
                    "push    r0                     \n\t"    \
                    "push    r1                     \n\t"    \
                    "clr    r1                      \n\t"    \
                    "push    r2                     \n\t"    \
                    "push    r3                     \n\t"    \
                    "push    r4                     \n\t"    \
                    "push    r5                     \n\t"    \
                    "push    r6                     \n\t"    \
                    "push    r7                     \n\t"    \
                    "push    r8                     \n\t"    \
                    "push    r9                     \n\t"    \
                    "push    r10                    \n\t"    \
                    "push    r11                    \n\t"    \
                    "push    r12                    \n\t"    \
                    "push    r13                    \n\t"    \
                    "push    r14                    \n\t"    \
                    "push    r15                    \n\t"    \
                    "push    r16                    \n\t"    \
                    "push    r17                    \n\t"    \
                    "push    r18                    \n\t"    \
                    "push    r19                    \n\t"    \
                    "push    r20                    \n\t"    \
                    "push    r21                    \n\t"    \
                    "push    r22                    \n\t"    \
                    "push    r23                    \n\t"    \
                    "push    r24                    \n\t"    \
                    "push    r25                    \n\t"    \
                    "push    r26                    \n\t"    \
                    "push    r27                    \n\t"    \
                    "push    r28                    \n\t"    \
                    "push    r29                    \n\t"    \
                    "push    r30                    \n\t"    \
                    "push    r31                    \n\t"    \
                    "in      %A0, __SP_L__           \n\t"    \
                    "in      %B0, __SP_H__           \n\t"    \
					: "=e" (threads.tcb->stack) : :\
                )
#endif
