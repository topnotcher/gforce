#include <stdint.h>
#include <avr/io.h>

#include <phasor_comm.h>
#include <mpc.h>


//#include "lights.h"
#include "commands.h"
#include "scheduler.h"
#include "display.h"
//#include "lcd.h"
//#include "sounds.h"
#include "game.h"
#include "lights.h"

volatile uint16_t game_time;
volatile uint8_t game_countdown_time;


//volatile uint8_t game_running = 0;

volatile game_state_t game_state = {
	.playing = 0,
	.stunned = 0,
	.active = 0
};

void ( *countdown_cb )(void);

inline void game_init() {
	//cmd_register(0x38, 11, &start_game_cmd);
	//cmd_register(0x08, 0, &stop_game_cmd);
}

void game_countdown() {
	static uint8_t data[] = "       ";

	if ( --game_countdown_time == 0 ) {
		countdown_cb();
	} else {
		data[6] = 0x30 + game_countdown_time;
		display_send(0,data,7);
	}
}

void start_game_cmd(command_t * cmd) {
	if ( game_state.playing )
		return;

	game_start_cmd * settings = (game_start_cmd *)cmd;
 
 	game_state.playing = 1;
	game_state.stunned = 0;
	game_state.active = 0;

	game_time = settings->seconds;
	game_countdown_time = settings->prestart+1;
	countdown_cb = &start_game_activate;

	scheduler_register(&game_countdown, 1000, game_countdown_time);
}

void game_tick() {
	if ( --game_time == 0 )
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

void stop_game_cmd( command_t * cmd ) {
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

	if ( game_countdown_time == 2 ) 
		mpc_send_cmd(0b0101,'B');
	
	else if ( game_countdown_time == 0 ) {
		lights_unstun();
		game_state.stunned = 0;
		game_state.active = 1;
		scheduler_unregister(&stun_timer);
	}
}

void do_stun() {
	
	if ( !game_state.active || !game_state.playing || game_state.stunned ) 
		return;

	game_state.stunned = 1;
	game_countdown_time = 4;
	
	lights_stun();

	scheduler_register(&stun_timer, 1000, game_countdown_time);
}

void do_deac() {

	if ( !game_state.active || !game_state.playing || game_state.stunned ) 
		return;

	lights_off();
//	sound_play_effect(SOUND_POWER_DOWN);
	
	game_countdown_time = 8;
//	countdown_cb = &/player_activate;
	scheduler_register(&game_countdown, 1000, game_countdown_time);
}


