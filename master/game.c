#include <stdint.h>
#include <avr/io.h>

#include "lights.h"
#include "scheduler.h"
//#include "lcd.h"
#include "sounds.h"
#include "game.h"

volatile uint16_t game_time;
volatile uint8_t game_countdown_time;
//volatile uint8_t game_running = 0;
volatile uint8_t stunned = 0;

volatile game_state_t game_state = {
	.playing = 0,
	.stunned = 0,
	.active = 0
};

/** 
 * @TODO keep status flags (stunned, active in a byte)
 * volatile uint8_t stunned = 0;
 */

void ( *countdown_cb )(void);


inline void game_init() {
	cmd_register(0x38, 11, &start_game_cmd);
	cmd_register(0x08, 0, &stop_game_cmd);
}

void game_countdown() {
	
	if ( --game_countdown_time == 0 ) {
		countdown_cb();
//		lcd_clear();
	} else {

/*		lcd_clear();
		lcd_putstring("       ");
		lcd_write( (0x30 + game_countdown_time) );*/
//		lcd_bl_on();

//		scheduler_register(&lcd_bl_off, SCHEDULER_HZ/8, 1);
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

	scheduler_register(&game_countdown, SCHEDULER_HZ, game_countdown_time);
}

void game_tick() {
	if ( --game_time == 0 )
		stop_game();
}

void start_game_activate() {
	scheduler_register(&game_tick, SCHEDULER_HZ, SCHEDULER_RUN_UNLIMITED);
	player_activate();
}

void player_activate() {
	
	/** 
	 * Because we'll get fucked sideways if someone gets deaced
	 * at the end of the game, then the game ends...
	 */
	if ( !game_state.playing )
		return;

	sound_play_effect(SOUND_POWER_UP);
	game_state.active = 1;
	lights_on();
}

void stop_game_cmd( command_t * cmd ) {
	stop_game();
}

void stop_game() {
	if ( game_state.playing ) {
		lights_off();
		sound_play_effect(SOUND_POWER_DOWN);
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
		lights_on();
	
	else if ( game_countdown_time == 0 ) {
		lights_unstun();
		game_state.stunned = 0;
		game_state.active = 1;
	}
}

void do_stun() {
	
	if ( !game_state.active || !game_state.playing || game_state.stunned ) 
		return;

	stunned = 1;
	game_countdown_time = 4;
	
	lights_off();
	lights_stun();

	scheduler_register(&stun_timer, SCHEDULER_HZ, 4);
}

void do_deac() {

	if ( !game_state.active || !game_state.playing || game_state.stunned ) 
		return;

	lights_off();
	sound_play_effect(SOUND_POWER_DOWN);
	
	//@TODO
	game_countdown_time = 8;
	countdown_cb = &player_activate;
	scheduler_register(&game_countdown, SCHEDULER_HZ, game_countdown_time);
}
