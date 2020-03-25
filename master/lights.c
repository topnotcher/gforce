#include <stdlib.h>
#include <stdint.h>
#include <mpc.h>
#include "lights.h"

#include <g4config.h>

static const uint8_t led_stun_pattern[] = {8, 0, 16, 16, 16, 16, 1, 6, 0, 2, 2, 2, 2, 1, 6, 0, 48, 48, 48, 48, 1, 6, 0, 6, 6, 6, 6, 1, 6, 0, 128, 128, 128, 128, 1, 6, 0, 15, 15, 15, 15, 1, 6, 0, 112, 112, 112, 112, 1, 6, 0, 5, 5, 5, 5, 1, 6, 0};
static const uint8_t led_stun_pattern_size = 58;


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

static const uint8_t led_active_pattern[] = {2, 0, 0xe, 0x0e, 0x0e, 0x0e, 1, 15, 0, 0xe0, 0xe0, 0xe0, 0xe0, 1, 15, 0};
static const uint8_t led_active_pattern_size = 16;

static const uint8_t led_countdown_pattern[] = {1, 0,
	0xee, 0xee, 0xee, 0xee, 1, 25, 85,
};

static void send_pattern(const uint8_t addr, const uint8_t size, const uint8_t *const pattern) {
	mpc_send(addr, MPC_CMD_LED_SET_SEQ, size, (uint8_t*)pattern);
}

void lights_on(void) {
	send_pattern(
		(MPC_ADDR_CHEST | MPC_ADDR_LS | MPC_ADDR_RS | MPC_ADDR_BACK | MPC_ADDR_PHASOR),
		led_active_pattern_size, led_active_pattern
	);
}

void lights_booting(const uint8_t addr) {
	send_pattern(addr, sizeof(led_booting), led_booting);
}

void lights_countdown(void) {
	send_pattern((MPC_ADDR_CHEST | MPC_ADDR_BACK), sizeof(led_countdown_pattern), led_countdown_pattern);
}


void lights_stun(void) {
	send_pattern(
		(MPC_ADDR_CHEST | MPC_ADDR_LS | MPC_ADDR_RS | MPC_ADDR_BACK | MPC_ADDR_PHASOR),
		led_stun_pattern_size, led_stun_pattern
	);
}

void lights_off(void) {
	mpc_send_cmd((MPC_ADDR_CHEST | MPC_ADDR_LS | MPC_ADDR_RS | MPC_ADDR_BACK | MPC_ADDR_PHASOR), MPC_CMD_LED_OFF);
}

void lights_unstun(void) {
	lights_on();
}

void lights_halfstun(void) {
	mpc_send_cmd(MPC_ADDR_BACK | MPC_ADDR_CHEST, MPC_CMD_LED_OFF);
}
