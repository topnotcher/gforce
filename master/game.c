#include <stdint.h>
#include <avr/io.h>

#include <phasor_comm.h>
#include <mpc.h>

#include "timer.h"
#include "display.h"
#include "sounds.h"
#include "game.h"
#include "lights.h"
#include "xbee.h"

#include <g4config.h>

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

//volatile uint16_t game_time;
volatile uint8_t game_countdown_time;

//volatile uint8_t game_running = 0;

volatile game_state_t game_state = {
	.playing = 0,
	.stunned = 0,
	.active = 0
};

QueueHandle_t game_evt_queue;

static struct {
	uint8_t deac_time;
	uint8_t stun_time;
	uint16_t game_time;
} game_settings;


enum game_event_type {
	GAME_EVT_CMD,
	GAME_EVT_STUN_TIMER,
	GAME_EVT_STOP_GAME,
	GAME_EVT_COUNTDOWN,
	GAME_EVT_TRIGGER,
};

typedef struct {
	enum game_event_type type;
	void *data;
} game_event;

void (* countdown_cb )(void);

static inline void handle_shot(const uint8_t, command_t const *const);
static void process_trigger_pkt(const mpc_pkt *const pkt);
static void __game_countdown(void);
static void stun_timer(void);
static void __stun_timer(void);
static void send_game_event(enum game_event_type, const void *const);
static void game_task(void *);
static void process_game_event(game_event *evt);
static void process_ir_pkt(const mpc_pkt *const pkt);
static void queue_ir_pkt(const mpc_pkt *const pkt);

inline void game_init(void) {
	mpc_register_cmd('I', queue_ir_pkt);
	mpc_register_cmd('T', process_trigger_pkt);


	game_evt_queue = xQueueCreate(8, sizeof(game_event));
	xTaskCreate(game_task, "game", 128, game_evt_queue, tskIDLE_PRIORITY + 6, (TaskHandle_t*)NULL);
}

static void game_task(void *params) {
	QueueHandle_t evt_queue = params;
	game_event evt;

	/* TODO: when power is applied, there is a race condition between the */
	/* display board coming up and this task starting. */
	display_write("GForce Booted");

	while (1) {
		if (xQueueReceive(evt_queue, &evt, portMAX_DELAY)) {
			process_game_event(&evt);
		}
	}
}

static void process_game_event(game_event *evt) {
	switch (evt->type) {
	case GAME_EVT_COUNTDOWN:
		__game_countdown();
		break;

	case GAME_EVT_STUN_TIMER:
		__stun_timer();
		break;

	case GAME_EVT_CMD:
		process_ir_pkt(evt->data);

	case GAME_EVT_TRIGGER:
		break;

	case GAME_EVT_STOP_GAME:
		stop_game();

	default:
		break;
	}
}

static void send_game_event(enum game_event_type type, const void *const data) {
	game_event evt = {
		.type = type,
		.data = (void*)data
	};

	xQueueSend(game_evt_queue, &evt, portMAX_DELAY);
}

void game_countdown(void) {
	send_game_event(GAME_EVT_COUNTDOWN, NULL);
}

static void __game_countdown(void) {
	static uint8_t data[] = "        ";

	if (--game_countdown_time == 0) {
		countdown_cb();
		data[7] = ' ';
	} else {
		data[7] = 0x30 + game_countdown_time;
	}
	display_send(0, 9, data);
}

void start_game_cmd(command_t const *const cmd) {
	if (game_state.playing)
		return;

	game_start_cmd *settings = (game_start_cmd *)cmd->data;

	game_state.playing = 1;
	game_state.stunned = 0;
	game_state.active = 0;

	game_settings.game_time = settings->seconds;
	game_settings.deac_time = settings->deac + 4;
	//@TODO disable stun option
	game_settings.stun_time = game_settings.deac_time / settings->stuns;

	game_countdown_time = settings->prestart + 1;
	countdown_cb = &start_game_activate;

	add_timer(&game_countdown, TIMER_HZ, game_countdown_time);
}

void game_tick(void) {
	if (--game_settings.game_time == 0)
		send_game_event(GAME_EVT_STOP_GAME, NULL);
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
	if (!game_state.playing)
		return;

	sound_play_effect(SOUND_POWER_UP);
	game_state.active = 1;

	lights_on();
}

void stop_game_cmd(command_t const *const cmd) {
	stop_game();
}

void stop_game(void) {
	if (!game_state.playing)
		return;

	lights_off();

	sound_play_effect(SOUND_POWER_DOWN);
	del_timer(&game_tick);

	display_write("Game Over");

	//slight issue here: if we stop game during countdown.
	del_timer(&game_countdown);
	game_state.playing = 0;
}

static void stun_timer(void) {
	send_game_event(GAME_EVT_STUN_TIMER, NULL);
}

static void __stun_timer(void) {

	if (!game_state.playing) {
		del_timer(stun_timer);
		return;
	}

	--game_countdown_time;

	//fuck shit remove this.
	if (game_countdown_time == (game_settings.stun_time >> 1))
		lights_halfstun();

	else if (game_countdown_time == 0) {
		lights_unstun();
		game_state.stunned = 0;
		game_state.active = 1;
		del_timer(&stun_timer);
		display_send(0, 2, (uint8_t *)" ");
	}
}

static inline void handle_shot(const uint8_t saddr, command_t const *const cmd) {

	if (!game_state.active || !game_state.playing || game_state.stunned)
		return;

	char *sensor;
	switch (saddr) {
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

	if (saddr & (MPC_BACK_ADDR | MPC_CHEST_ADDR))
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

static void queue_ir_pkt(const mpc_pkt *const pkt) {
	mpc_incref(pkt);
	send_game_event(GAME_EVT_CMD, pkt);
}

static void process_ir_pkt(const mpc_pkt *const pkt) {

	const command_t *const cmd = (command_t *)pkt->data;

	if (cmd->cmd == 0x38) {
		start_game_cmd(cmd);

	} else if (cmd->cmd == 0x08) {
		stop_game_cmd(cmd);

	} else if (cmd->cmd == 0x0c) {
		handle_shot(pkt->saddr, cmd);

		xbee_send('S', sizeof(*pkt) + pkt->len, (uint8_t *)pkt);
	}

	mpc_decref(pkt);
}

void process_trigger_pkt(const mpc_pkt *const pkt) {
	// size, repeat, data...
	static uint8_t data[] = {5, 3, 0xff, 0x0c, 0x63, 0x88, 0xa6};

	if (game_state.active) {
		sound_play_effect(SOUND_LASER);
		mpc_send(MPC_PHASOR_ADDR, 'T', sizeof(data), data);
	}
}
