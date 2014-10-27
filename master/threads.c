#include <stdint.h>
#include <util/atomic.h>
#include "threads.h"

static void * thread_stack_init(uint8_t * stack, void (*task)(void)); 

threads_t threads;

uint8_t thread_create(const char * name, void (*task)(void)) {
	tcb_t * tcb;
	uint8_t pid;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		pid = threads.num++;
		tcb = &threads.list[pid];
		tcb->name = name;
		tcb->pid = pid;
		//new thread goes on the current top of stack
		tcb->stack = thread_stack_init(threads.top_of_stack, task);
		threads.top_of_stack = (void*)((uint16_t)tcb->stack - (uint16_t)THREADS_STACK_SIZE); 
	}

	return pid; 
}

void * thread_stack_init(uint8_t * stack, void (*task)(void) ) {
	*stack = 0x11; //38
	stack--;
	*stack = 0x22; //37
	stack--;
	*stack = 0x33; //36
	stack--;

	uint16_t addr = (uint16_t)task;

	*stack = (addr&0xff); //35
	stack--;
	addr >>= 8;

	*stack = (addr&0xff); //34
	stack--;

#if defined(__AVR_3_BYTE_PC__) && __AVR_3_BYTE_PC__
	*stack = 0; 
	stack--;
#endif

	*stack = 0x00; /*R0*/
	stack--;

	*stack = 0x80; /*SREG*/
	stack--;

	*stack = 0x00; /*R1*/
	stack--;
	
	*stack = 0x02; /*R2*/
	stack--;

	*stack = 0x03; /*R3*/
	stack--;

	*stack = 0x04; /*R4*/
	stack--;

	*stack = 0x05; /*R5*/
	stack--;

	*stack = 0x06; /*R6*/
	stack--;

	*stack = 0x07; /*R7*/
	stack--;

	*stack = 0x08; /*R8*/
	stack--;

	*stack = 0x09; /*R9*/
	stack--;

	*stack = 0x0a; /*R10*/
	stack--;

	*stack = 0x0b; /*R11*/
	stack--;

	*stack = 0x0c; /*R12*/
	stack--;

	*stack = 0x0d; /*R13*/
	stack--;

	*stack = 0x0e; /*R14*/
	stack--;

	*stack = 0x0f; /*R15*/
	stack--;

	*stack = 0x10; /*R16*/
	stack--;

	*stack = 0x11; /*R17*/
	stack--;

	*stack = 0x12; /*R18*/
	stack--;

	*stack = 0x13; /*R19*/
	stack--;

	*stack = 0x14; /*R20*/
	stack--;

	*stack = 0x15; /*R21*/
	stack--;

	*stack = 0x16; /*R22*/
	stack--;

	*stack = 0x17; /*R23*/
	stack--;

	*stack = 0x18; /*R24*/
	stack--;

	*stack = 0x19; /*R25*/
	stack--;

	*stack = 0x1a; /*R26*/
	stack--;

	*stack = 0x1b; /*R27*/
	stack--;

	*stack = 0x1c; /*R28*/
	stack--;

	*stack = 0x1d; /*R29*/
	stack--;

	*stack = 0x1e; /*R30*/
	stack--;

	*stack = 0x1f; /*R31*/
	stack--;

	return stack;
}
