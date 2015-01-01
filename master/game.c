#include <stdint.h>
#include <avr/io.h>

#include <phasor_comm.h>
#include <mpc.h>
#include <tasks.h>


#include "timer.h"
#include "display.h"
#include "sounds.h"
#include "game.h"
#include "lights.h"
#include "xbee.h"

#include <g4config.h>

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
static void process_trigger_pkt(const mpc_pkt * const pkt);
static void __game_countdown(void);
static void stun_timer(void);
static void __stun_timer(void);

inline void game_init(void) {
	mpc_register_cmd('I', process_ir_pkt);
	mpc_register_cmd('T', process_trigger_pkt);
}

void game_countdown(void) {
	task_schedule(__game_countdown);
}

static void __game_countdown(void) {
	static uint8_t data[] = "        ";

	if ( --game_countdown_time == 0 ) {
		countdown_cb();
		data[7] = ' ';
	} else {
		data[7] = 0x30 + game_countdown_time;
	}
	display_send(0,9,data);
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

	add_timer(&game_countdown, TIMER_HZ, game_countdown_time);
}

void game_tick(void) {
	if ( --game_settings.game_time == 0 )
		stop_game();
}

void start_game_activate(void) {
	add_timer(&game_tick, TIMER_HZ, TIMER_RUN_UNLIMITED);
	player_activate();
}

void player_activate(void) {
	
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

void stop_game_cmd( command_t const * const cmd ) {
	stop_game();
}

void stop_game(void) {
	if ( game_state.playing ) {
		lights_off();

		sound_play_effect(SOUND_POWER_DOWN);
		del_timer(&game_tick);

		display_write("            ");

		//slight issue here: if we stop game during countdown.
		del_timer(&game_countdown);
//		lcd_clear();
		game_state.playing = 0;
	}
}

static void stun_timer(void) {
	task_schedule(__stun_timer);
}

static void __stun_timer(void) {


	if (!game_state.playing) {
		del_timer(stun_timer);
		return;
	}

	--game_countdown_time;

	//fuck shit remove this.
	if ( game_countdown_time == (game_settings.stun_time>>1) )
		lights_halfstun();
	
	else if ( game_countdown_time == 0 ) {
		lights_unstun();
		game_state.stunned = 0;
		game_state.active = 1;
		del_timer(&stun_timer);
		display_send(0,2,(uint8_t*)" ");
	}
}

static inline void handle_shot(const uint8_t saddr, command_t const * const cmd) {

	if ( !game_state.active || !game_state.playing || game_state.stunned ) 
		return;

	char * sensor;
	switch(saddr) {
	case MPC_CHEST_ADDR:
		sensor = "Tagged: CH";
		break;
	case MPC_LS_ADDR:
		sensor = "Tagged: LS";
		break;
	case MPC_BACK_ADDR:
		sensor = "Tagged: BA";
		break;
	case MPC_RS_ADDR: 
		sensor = "Tagged: RS";
		break;
	case MPC_PHASOR_ADDR:
		sensor = "Tagged: PH";
		break;
	default:
		sensor = "Tagged: ??";
			break;
	}

	if ( saddr & (MPC_BACK_ADDR | MPC_CHEST_ADDR) )
		do_deac();
	else
		do_stun();

	display_write(sensor);

	//derpderpderp
}


void do_stun(void) {
	
	game_state.stunned = 1;
	game_countdown_time = game_settings.stun_time;
	
	lights_stun();

	add_timer(&stun_timer, TIMER_HZ, game_countdown_time);
}

void do_deac(void) {
	game_state.active = 0;
	lights_off();
	sound_play_effect(SOUND_POWER_DOWN);
	
	game_countdown_time = game_settings.deac_time;
	countdown_cb = player_activate;
	add_timer(&game_countdown, TIMER_HZ, game_countdown_time);
}

void process_ir_pkt(mpc_pkt const * const pkt) {

	command_t const * const  cmd = (command_t*)pkt->data;

	if ( cmd->cmd == 0x38) {
		start_game_cmd(cmd);	

	} else if ( cmd->cmd == 0x08 )  {
		stop_game_cmd(cmd);

	} else if ( cmd->cmd == 0x0c ) {
		handle_shot(pkt->saddr, cmd);	

		xbee_send('S',sizeof(*pkt)+pkt->len, (uint8_t*)pkt);
	}
}

void process_trigger_pkt(const mpc_pkt * const pkt) {
	static uint8_t data[] = {16,3,255, 56, 127 ,138,103,83,0,15,15,68,72,0,44,1,88,113};
	if (game_state.active) {
		sound_play_effect(SOUND_LASER);
		mpc_send(MPC_PHASOR_ADDR, 'T', 18, data);
	}
}
