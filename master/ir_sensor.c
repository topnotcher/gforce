#include <string.h>
#include <avr/io.h>
#include <stdint.h>

#include "ir_sensor.h"
#include "game.h"
#include "util.h"

//used during debugging
#include <util/delay.h>
#include <stdio.h>
#include "lcd.h"

ir_sensor_state state;

void ir_sensor_init() {
	//this is completely unnecessary
	IR_SENSOR_DDR &= ~( _BV(IR_SENSOR_CHEST) | _BV(IR_SENSOR_BACK) | _BV(IR_SENSOR_SHOULDER) | _BV(IR_SENSOR_PHASOR) );

	//enable pullup
	IR_SENSOR_PORT |= _BV(IR_SENSOR_CHEST) | _BV(IR_SENSOR_BACK) | _BV(IR_SENSOR_SHOULDER) | _BV(IR_SENSOR_PHASOR);

	//enable pin change interrupts 
	//going to need to abstract pin change interrupts in hardware when
	//multiple components need the same ISR...
	ir_interrupt_enable();

	IR_SENSOR_PCICR |= _BV(IR_SENSOR_PCIE);
	
	memset(&state, 0, sizeof(state));
	
	//and explicitly verify the we start off with *no* state.
	state.recv_state = ir_state_none;
}

void ir_interrupt_enable() {
	IR_SENSOR_PCMSK |= _BV(IR_SENSOR_CHEST) | _BV(IR_SENSOR_BACK) | _BV(IR_SENSOR_SHOULDER) | _BV(IR_SENSOR_PHASOR);
}

void ir_interrupt_disable() {
	IR_SENSOR_PCMSK &= ~( _BV(IR_SENSOR_CHEST) | _BV(IR_SENSOR_BACK) | _BV(IR_SENSOR_SHOULDER) | _BV(IR_SENSOR_PHASOR) );
}

void ir_state_clear() {
	state.recv_state = ir_state_none;
	state.dirty = 1;
	scheduler_unregister(&ir_sensor_tick);
	ir_interrupt_enable();
}

inline void ir_state_set(enum ir_sensor_recv_state new_state) {
	state.recv_state = new_state;
	state.bits = 0; 
	state.ticks = 0;
}

inline void ir_recv_init() {

	if ( state.recv_state == ir_state_none ) {
		if ( !IR_SENSOR_READ(CHEST) )
			state.sensor = IR_SENSOR_CHEST;

		else if ( !IR_SENSOR_READ(BACK) )
			state.sensor = IR_SENSOR_BACK;

		else if ( !IR_SENSOR_READ(SHOULDER)  )
			state.sensor = IR_SENSOR_SHOULDER;

		else if ( !IR_SENSOR_READ(PHASOR) )
			state.sensor = IR_SENSOR_PHASOR;
		else {
			return;
		}

		state.bytes = 0;

	} else if ( IR_SENSOR_VALUE(state.sensor) ) 
		return;			

	//otherwise state should be set to stop.

	ir_interrupt_disable();

	//the pin must be pulled low if we're hitting this.
	state.pin_value = 0;

	ir_state_set(ir_state_start);

	// this is a special case.
	// we hit it at the start of the first bit, 
	// and we don't give a fuck again until the midpoint of the second bit.
	scheduler_register(&ir_sensor_tick, (.5*IR_PULSE_TICKS) ,1);
}

void ir_sensor_tick() {

	if ( state.pin_value != IR_SENSOR_VALUE(state.sensor) ) {
		state.bits++;
		state.ticks = 1;
		state.pin_value = IR_SENSOR_VALUE(state.sensor);
	} else {
		state.ticks++;
		state.bits++;
	}
	
	switch( state.recv_state ) {

	/**
	 * ir_state_parity: Not really a parity bit. bit is set after the 4th byte.
	 */
	case ir_state_parity:
		if ( state.bits == IR_SENSOR_PARITY_BITS ) {
			ir_state_set(ir_state_stop);
			state.parity = state.pin_value;
		}
		else if ( state.bits > IR_SENSOR_PARITY_BITS ) {
			ir_state_clear();
		
		}

		break; /* ir_state_parity */

	/**
	 * ir_state_stop: the N stop bits.
	 * handles all command dispatching, verification, etc 
	 */
	case ir_state_stop:
		//do not set state here. We wait for the damn interrupt.

		/** 
		 * receive a starte byte
		 */
		if ( !state.parity && state.buf[state.bytes-1] == 0xFF )  {
			state.bytes = 0;
			state.dirty = 0;

			//@TODO: crc
			state.shift = 0x55;

		/**
		 * My shit's all stanky: receive data without preceding start byte.
		 */
		} else if ( state.dirty ) {
			ir_state_clear();

		/**
		 * Receive a good byte, part of packet header
		 */
		} else if ( state.bytes < sizeof(command_t) ) {
			crc(&state.shift, state.buf[ state.bytes - 1 ], IR_CRC_POLY );

		/**
		 * Receive a full header -- check CRC
		 */
		} else if ( state.bytes == sizeof(command_t) ) {

			//crc mismatch
			if ( ( (command_t *) state.buf )->crc != state.shift ) {
				ir_state_clear();
				break;
			}

			state.args = cmd_n_args(state.buf[0]);

			if ( state.args == 0 ) {
				cmd_dispatch(state.buf[0], (command_t *)state.buf);
				ir_state_clear();
				break;
			}

		/**
		 * Receive bytes after packet header (e.g. start command)
		 */
		} else if ( (state.bytes-sizeof(command_t)) < state.args ) {
			crc(&state.shift, state.buf[ state.bytes - 1 ], IR_CRC_POLY);

		/**
		 * Done receiving bytes after header.
		 */
		} else if ( (state.bytes - sizeof(command_t)) == state.args ) {

			//the second CRC matches
			if ( state.shift == state.buf[ state.bytes - 1 ] ) 
				cmd_dispatch(state.buf[0], (command_t *)state.buf);

			ir_state_clear();
			break;

		} else if ( state.bytes == IR_SENSOR_MAX_BYTES ) {

			/** 
			 * DIE: tried to receive an oversized command (not even possible)
			 */
			ir_state_clear();
			break;
		}
		

		/**
		 * if we didn't bail, wait for another byte.
		 * regardless, this won't hurt anything (just more efficient
		 * since ir_state_clear() does this anyway.)
		 */
		ir_interrupt_enable();

		break; /* ir_state_stop */

	/**
	 * ir_state_start: waiting for the start bit(s)? 
	 */
	case ir_state_start:
		if ( state.pin_value ) {
			// unlikely case since we'll only hit this after an interrupt.
			ir_state_clear();
		} else if ( state.bits == IR_SENSOR_START_BITS ) {
			//0 11111111 0 1<--stop here1 
			scheduler_register(&ir_sensor_tick, IR_PULSE_TICKS, IR_SENSOR_DATA_BITS + IR_SENSOR_PARITY_BITS + 1);

			ir_state_set(ir_state_data);
			state.buf[state.bytes] = 0;

		} 

		break; /* ir_state_start */

	/**
	 * ir_state_data: the actual data bytes in the frame
	 */
	case ir_state_data:
		//if the value is 1, we need to shift the value into the buffer.
		if (state.pin_value)
			state.buf[state.bytes] |= (1<<(state.bits-1));

		if ( state.bits == IR_SENSOR_DATA_BITS ) {
			ir_state_set(ir_state_parity);
			state.bytes++;
		}

		break; /* ir_state_data */

	default: 
		ir_state_clear();

		break; /* default */
	}
}

//suck my dick
IR_SENSOR_PCISR {
	ir_recv_init();
}
