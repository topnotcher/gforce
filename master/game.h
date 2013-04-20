#include "commands.h"

#ifndef GAME_H
#define GAME_H

#define GAME_SECONDS 10
#define GAME_START_SECONDS 3

//shit isn't portable. deal.
typedef struct {
	command_t hdr;

	uint8_t passkey2;

	//game number
	uint8_t number;

	uint8_t lives:4;
	uint8_t rlivesat:4;

	uint8_t ammo:4;
	uint8_t rammoat:4;

	uint8_t deac:3;
	uint8_t stuns:5;
	
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

} game_start_cmd;

typedef struct {
	uint8_t playing:1;
	uint8_t active:1;
	uint8_t stunned:1;
} game_state_t;

void game_countdown(void);
void stop_game();
void stop_game_cmd(command_t *);
void game_tick(void);

void start_game_cmd(command_t *);
void start_game_activate(void);

void player_activate(void);

void do_stun(void);
void do_deac(void);

void stun_timer(void);

void game_init(void);

#endif