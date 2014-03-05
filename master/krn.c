#include <stdint.h>
#include "krn.h"
void * task_stack_init(uint8_t * stack, void (*task)(void) ) {
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
