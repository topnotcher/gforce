#include <mpc.h>
#include <stdint.h>

#ifndef GAME_H
#define GAME_H

#define GAME_SECONDS 10
#define GAME_START_SECONDS 3

typedef struct {
	uint8_t cmd;
	uint8_t packs;
	uint8_t size;
	uint8_t crc;

	uint8_t data[];
} __attribute__((packed)) command_t;

typedef struct {
	uint8_t passkey2;

	//game number
	uint8_t number;

	uint8_t lives:4;
	uint8_t rlivesat:4;

	uint8_t ammo:4;
	uint8_t rammoat:4;

	uint8_t deac:5;
	uint8_t stuns:3;
	
	//game type e.g. solo
	uint8_t mode:3;
	uint8_t prestart:5;
	
	//firing rate
	uint8_t reloads:5;
	uint8_t fire:3;

	//number of seconds.
	//different endianness would cause anal bleeding.
	uint16_t seconds;

	//3rd bit in reload config da faq?
	uint8_t reloads3:1; 
	uint8_t b2_unused:1;
	//rp time reduction disable.
	uint8_t rp_tr:1;
	//rm menu disable
	uint8_t rp:1;
	uint8_t defender:1;
	//hand sensor disable.
	uint8_t hands:1;
	//two more bits here. Rp menu something and ??


	uint8_t crc;

} __attribute__((packed)) game_start_cmd;

enum game_event_type {
	GAME_EVT_IR_CMD,
	GAME_EVT_STUN_TIMER,
	GAME_EVT_STOP_GAME,
	GAME_EVT_COUNTDOWN,
	GAME_EVT_TRIGGER,
	GAME_EVT_BOOTED,
	GAME_EVT_MEMBER_LOGIN,
};

void game_init(void);
void send_game_event(enum game_event_type, const void *const);

#endif
