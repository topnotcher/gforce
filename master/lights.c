#include <stdlib.h>
#include <stdint.h>
#include <mpc.h>
#include <phasor_comm.h>
#include "lights.h"

#include <g4config.h>

static uint8_t led_stun_pattern[] = {8, 0, 16, 16, 16, 16, 1, 6, 0, 2, 2, 2, 2, 1, 6, 0, 48, 48, 48, 48, 1, 6, 0, 6, 6, 6, 6, 1, 6, 0, 128, 128, 128, 128, 1, 6, 0, 15, 15, 15, 15, 1, 6, 0, 112, 112, 112, 112, 1, 6, 0, 5, 5, 5, 5, 1, 6, 0};
static uint8_t led_stun_pattern_size = 58;


static uint8_t led_booting[] = {8, 0,
	0x10, 0x00, 0x00, 0x00, 1, 17, 0,
	0x02, 0x00, 0x00, 0x00, 1, 17, 0,

	0x00, 0x30, 0x00, 0x00, 1, 17, 0,
	0x00, 0x04, 0x00, 0x00, 1, 17, 0,

	0x00, 0x00, 0x50, 0x00, 1, 17, 0,
	0x00, 0x00, 0x06, 0x00, 1, 17, 0,

	0x00, 0x00, 0x00, 0x70, 1, 17, 0,
	0x00, 0x00, 0x00, 0x08, 1, 17, 0,
};

static uint8_t led_active_pattern[] = {2, 0, 0xe, 0x0e, 0x0e, 0x0e, 1, 15, 0, 0xe0, 0xe0, 0xe0, 0xe0, 1, 15, 0};
static uint8_t led_active_pattern_size = 16;


inline void lights_on(void) {
	mpc_send((MPC_CHEST_ADDR | MPC_LS_ADDR | MPC_RS_ADDR | MPC_BACK_ADDR | MPC_PHASOR_ADDR), 'A', led_active_pattern_size, led_active_pattern);
}

void lights_booting(const uint8_t addr) {
	mpc_send(addr, 'A', sizeof(led_booting), led_booting);
}

inline void lights_stun(void) {
	mpc_send((MPC_CHEST_ADDR | MPC_LS_ADDR | MPC_RS_ADDR | MPC_BACK_ADDR | MPC_PHASOR_ADDR), 'A', led_stun_pattern_size, led_stun_pattern);
}

inline void lights_off(void) {
	mpc_send_cmd((MPC_CHEST_ADDR | MPC_LS_ADDR | MPC_RS_ADDR | MPC_BACK_ADDR | MPC_PHASOR_ADDR), 'B');
}

inline void lights_unstun(void) {
	lights_on();
}

inline void lights_halfstun(void) {
	mpc_send_cmd(MPC_BACK_ADDR | MPC_CHEST_ADDR, 'B');
}
