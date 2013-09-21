#include <stdlib.h>
#include <stdint.h>
#include <mpc.h>
#include <phasor_comm.h>
#include "lights.h"

static uint8_t led_stun_pattern[] = {8,0,16,16,16,16,1,6,0,2,2,2,2,1,6,0,48,48,48,48,1,6,0,6,6,6,6,1,6,0,128,128,128,128,1,6,0,15,15,15,15,1,6,0,112,112,112,112,1,6,0,5,5,5,5,1,6,0};
static uint8_t led_stun_pattern_size = 58;

static uint8_t led_active_pattern[] = {2,0, 0x1,0x01,0x01,0x01, 1,15,0, 0x10,0x10,0x10,0x10, 1,15,0 };
static uint8_t led_active_pattern_size = 16;


inline void lights_on(void) {
	mpc_send(0b1111,'A', led_active_pattern_size, led_active_pattern);
	phasor_comm_send('A', led_active_pattern_size, led_active_pattern);

}

inline void lights_stun(void) {
	mpc_send(0b1111,'A', led_stun_pattern_size, led_stun_pattern);
	phasor_comm_send('A',led_stun_pattern_size, led_stun_pattern);

}

inline void lights_off(void) {
	phasor_comm_send('B',0,NULL);
	mpc_send_cmd(0b1111,'B');
}

inline void lights_unstun(void) {
	lights_on();
}
