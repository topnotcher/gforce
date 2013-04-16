#include <stdint.h>

#include "ports.h"
#include "scheduler.h"

#ifndef IR_SENSOR_H
#define IR_SENSOR_H


/**
 * The port (A,B, etc) to which the IR sensors are connected
 */
#define IR_SENSOR_PORTID A

/**
 * pin # for the sensor.s
 * e.g. if they're on port A (IR_SENSOR_PORTID)
 * and the chest is PA0, then IR_SENSOR_CHEST = 0
 */
#define IR_SENSOR_CHEST 0
#define IR_SENSOR_SHOULDER 1
#define IR_SENSOR_BACK 2
#define IR_SENSOR_PHASOR 3 

#define IR_SENSOR_PORT PORT_ID(IR_SENSOR_PORTID)
#define IR_SENSOR_DDR DDR_ID(IR_SENSOR_PORTID)
#define IR_SENSOR_PIN PIN_ID(IR_SENSOR_PORTID)

#define IR_SENSOR_READ(sensor) (IR_SENSOR_PIN&_BV(PORT_CONCAT(IR_SENSOR_,sensor)))

//same thing by pin #
#define IR_SENSOR_VALUE(sensor) (IR_SENSOR_PIN&_BV(sensor))


/**
 * Configuration for pin-change interrupts.
 */
#define IR_SENSOR_PCMSK PCMSK0
#define IR_SENSOR_PCICR PCICR
#define IR_SENSOR_PCIE PCIE0

#define IR_SENSOR_PCISR ISR(PCINT0_vect) 

#define IR_BAUD_RATE 1515.762

//number of fucking scheduler tits per bit.
#define IR_PULSE_TICKS (SCHEDULER_HZ/IR_BAUD_RATE)

#define IR_SENSOR_START_BITS 1
#define IR_SENSOR_STOP_BITS 2
#define IR_SENSOR_PARITY_BITS 1
#define IR_SENSOR_DATA_BITS 8

#define IR_SENSOR_MAX_BYTES 16

#define IR_CRC_POLY 0x24

enum ir_sensor_recv_state { 
	ir_state_none,

	//in the data bits.
	ir_state_data,

	//in the parity bit. Well, this bit isn't really
	//used for parity, but whatever... It's the 9th bit.
	ir_state_parity,

	//in the stop bits.
	ir_state_stop,

	ir_state_start
};

typedef struct {
	// sensor being read
	uint8_t sensor;

	// current state.
	enum ir_sensor_recv_state recv_state;

	//number of bits read in current state.
	uint8_t bits;

	//number of ticks sensor has gone without changing value.
	uint8_t ticks;

	uint8_t pin_value;

	uint8_t buf[IR_SENSOR_MAX_BYTES];
	
	// number of bytes we've read.
	// this also represents the index of buf to which
	// we are writing.
	uint8_t bytes;

	uint8_t parity;

	//state is clean while receiving a valid command
	//once dirty, dirty until start byte
	uint8_t dirty;

	uint8_t shift;

	//size of current command (beyond command_t)
	uint8_t args;

} ir_sensor_state;

void ir_sensor_init(void);
void ir_interrupt_enable(void);
void ir_interrupt_disable(void);
void ir_state_clear(void);
void ir_state_set(enum ir_sensor_recv_state);
void ir_recv_init(void);
void ir_sensor_tick(void);

#endif
