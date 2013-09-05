#include <stdint.h>
#include <avr/io.h>

#include <phasor_comm.h>
#include <mpc.h>


#include "scheduler.h"
#include "display.h"
//#include "lcd.h"
//#include "sounds.h"
#include "game.h"
#include "lights.h"
#include "xbee.h"

//volatile uint16_t game_time;
volatile uint8_t game_countdown_time;

//volatile uint8_t game_running = 0;

volatile game_state_t game_state = {
	.playing = 0,
	.stunned = 0,
	.active = 0
};

static struct { 
	uint8_t deac_time;
	uint8_t stun_time;
	uint16_t game_time;
} game_settings;


void ( *countdown_cb )(void);

static inline void handle_shot(const uint8_t, command_t const * const);

inline void game_init() {
	//cmd_register(0x38, 11, &start_game_cmd);
	//cmd_register(0x08, 0, &stop_game_cmd);
}

void game_countdown() {
	static uint8_t data[] = "       ";

	if ( --game_countdown_time == 0 ) {
		countdown_cb();
		data[6] = ' ';
	} else {
		data[6] = 0x30 + game_countdown_time;
	}
	display_send(0,data,8);
}

void start_game_cmd(command_t const * const cmd) {
	if ( game_state.playing )
		return;

	game_start_cmd * settings = (game_start_cmd *)cmd;
 
 	game_state.playing = 1;
	game_state.stunned = 0;
	game_state.active = 0;

	game_settings.game_time = settings->seconds;
	game_settings.deac_time = settings->deac+4;
	//@TODO disable stun option
	game_settings.stun_time = game_settings.deac_time/settings->stuns;

	game_countdown_time = settings->prestart+1;
	countdown_cb = &start_game_activate;

	scheduler_register(&game_countdown, 1000, game_countdown_time);
}

void game_tick() {
	if ( --game_settings.game_time == 0 )
		stop_game();
}

void start_game_activate() {
	scheduler_register(&game_tick, 1000, SCHEDULER_RUN_UNLIMITED);
	player_activate();
}

void player_activate() {
	
	/** 
	 * Because we'll get fucked sideways if someone gets deaced
	 * at the end of the game, then the game ends...
	 */
	if ( !game_state.playing )
		return;

//	sound_play_effect(SOUND_POWER_UP);
	game_state.active = 1;

	lights_on();
}

void stop_game_cmd( command_t const * const cmd ) {
	stop_game();
}

void stop_game() {
	if ( game_state.playing ) {
		lights_off();
//		sound_play_effect(SOUND_POWER_DOWN);
		scheduler_unregister(&game_tick);

		//slight issue here: if we stop game during countdown.
		scheduler_unregister(&game_countdown);
//		lcd_clear();
		game_state.playing = 0;
	}
}

void stun_timer() {

	--game_countdown_time;

	if ( game_countdown_time == (game_settings.stun_time>>1) ) 
		mpc_send_cmd(0b0101,'B');
	
	else if ( game_countdown_time == 0 ) {
		lights_unstun();
		game_state.stunned = 0;
		game_state.active = 1;
		scheduler_unregister(&stun_timer);
		display_send(0,(uint8_t*)" ",2);
	}
}

static inline void handle_shot(const uint8_t saddr, command_t const * const cmd) {

	if ( !game_state.active || !game_state.playing || game_state.stunned ) 
		return;

	char * sensor;
	switch(saddr) {
	case 2:
		sensor = "Tagged: CH";
		break;
	case 4:
		sensor = "Tagged: LS";
		break;
	case 8:
		sensor = "Tagged: BA";
		break;
	case 16: 
		sensor = "Tagged: RS";
		break;
	case 32:
		sensor = "Tagged: PH";
		break;
	default:
		sensor = "Tagged: ??";
			break;
	}

	if ( saddr & (8 | 2) )
		do_deac();
	else
		do_stun();

	display_write(sensor);

	//derpderpderp
}


void do_stun() {
	
	game_state.stunned = 1;
	game_countdown_time = game_settings.stun_time;
	
	lights_stun();

	scheduler_register(&stun_timer, 1000, game_countdown_time);
}

void do_deac() {

	lights_off();
//	sound_play_effect(SOUND_POWER_DOWN);
	
	game_countdown_time = game_settings.deac_time;
//	countdown_cb = &/player_activate;
	scheduler_register(&game_countdown, 1000, game_countdown_time);
}

inline void process_ir_pkt(mpc_pkt const * const pkt) {

	command_t const * const  cmd = (command_t*)pkt->data;

	if ( cmd->cmd == 0x38) {
		start_game_cmd(cmd);	

	} else if ( cmd->cmd == 0x08 )  {
		stop_game_cmd(cmd);

	} else if ( cmd->cmd == 0x0c ) {
		handle_shot(pkt->saddr, cmd);
	}

	xbee_send('S', (uint8_t*)pkt, sizeof(*pkt)+pkt->len);
}
