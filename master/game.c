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

// Number of seconds to spend waiting for boards
#define GFORCE_BOOT_TICKS 10

typedef struct {
	enum game_event_type type;
	void *data;
} game_event;

typedef struct {
	uint8_t playing:1;
	uint8_t active:1;
	uint8_t stunned:1;
} game_state_t;

static uint8_t game_countdown_time;
static game_state_t game_state;
static QueueHandle_t game_evt_queue;

static struct {
	uint8_t deac_time;
	uint8_t stun_time;
	uint16_t game_time;
} game_settings;

void (* countdown_cb )(void);

const uint8_t all_boards = MPC_LS_ADDR | MPC_RS_ADDR | MPC_CHEST_ADDR | MPC_BACK_ADDR | MPC_PHASOR_ADDR;
static uint8_t boards_booted;
static uint8_t gforce_booted;

static void game_task(void *);
static void process_game_event(game_event *evt);

static void process_trigger(void);
static void trigger_event(const mpc_pkt *const);
static void process_ir_pkt(const mpc_pkt *const);
static void queue_ir_pkt(const mpc_pkt *const);

static void start_countdown(const uint8_t, void (*)(void));
static void game_countdown(void);
static void __game_countdown(void);
static void stun_timer(void);
static void __stun_timer(void);

static void handle_shot(const uint8_t, command_t const *const);
static void do_stun(void);
static void do_deac(void);
static void start_game_cmd(command_t const *const);
static void stop_game_cmd(command_t const *const);
static void stop_game(void);

static void game_tick(void);
static void start_game_activate(void);
static void player_activate(void);

static void gforce_boot_tick(void);
static void _gforce_boot_tick(void);
static void mpc_hello_received(const mpc_pkt *const);
static void handle_board_hello(const mpc_pkt *const);


void game_init(void) {
	game_evt_queue = xQueueCreate(8, sizeof(game_event));
	xTaskCreate(game_task, "game", 256, game_evt_queue, tskIDLE_PRIORITY + 6, (TaskHandle_t*)NULL);
}

static void game_task(void *params) {
	QueueHandle_t evt_queue = params;
	game_event evt;

	game_state.playing = 0;
	game_state.active = 0;
	game_state.stunned = 0;

	mpc_register_cmd(MPC_CMD_BOARD_HELLO, mpc_hello_received);

	/**
	 * Add a 10 second timer to wait for all boards to come up.
	 */
	add_timer(&gforce_boot_tick, TIMER_HZ, GFORCE_BOOT_TICKS);

	while (1) {
		if (xQueueReceive(evt_queue, &evt, portMAX_DELAY)) {
			process_game_event(&evt);
		}
	}
}

static void gforce_boot_tick(void) {
	send_game_event(GAME_EVT_BOOT_TICK, NULL);
}

static void _gforce_boot_tick(void) {
	static uint8_t ticks = GFORCE_BOOT_TICKS;

	ticks--;

	// timeout when the tick count  expires
	// but allow boot complete at GFORCE_BOOT_TICKS/2
	if (ticks == 0 || (ticks < GFORCE_BOOT_TICKS/2 && boards_booted == all_boards)) {
		mpc_register_cmd(MPC_CMD_IR_RX, queue_ir_pkt);
		mpc_register_cmd(MPC_CMD_IR_TX, trigger_event);

		if (boards_booted == all_boards)
			display_write("GForce Booted");
		else
			display_write("Error: \nBoards missing!");

		lights_off();

		gforce_booted = 1;

	} else {
		if (all_boards ^ boards_booted)
			mpc_send_cmd(all_boards ^ boards_booted, MPC_CMD_BOARD_HELLO);

		display_write("Booting...      ");
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

	case GAME_EVT_IR_CMD:
		process_ir_pkt(evt->data);
		break;

	case GAME_EVT_TRIGGER:
		process_trigger();
		break;

	case GAME_EVT_STOP_GAME:
		stop_game();
		break;

	case GAME_EVT_BOOT_TICK:
		_gforce_boot_tick();
		break;

	case GAME_EVT_MEMBER_LOGIN:
		display_write("Welcome");
		break;

	case GAME_EVT_BOARD_HELLO:
		handle_board_hello(evt->data);
		break;

	default:
		break;
	}
}

void send_game_event(enum game_event_type type, const void *const data) {
	game_event evt = {
		.type = type,
		.data = (void*)data
	};

	xQueueSend(game_evt_queue, &evt, portMAX_DELAY);
}

static void game_countdown(void) {
	send_game_event(GAME_EVT_COUNTDOWN, NULL);
}

static void __game_countdown(void) {
	static uint8_t data[] = "        ";

	if (--game_countdown_time == 0) {
		countdown_cb();
		countdown_cb = NULL;

		data[7] = ' ';
	} else {
		data[7] = 0x30 + game_countdown_time;
	}

	display_send(0, 9, data);
}

/**
 * Start the game in response to a game start command received from IR. The
 * command contains the game settings.
 */
static void start_game_cmd(command_t const *const cmd) {
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

	start_countdown(settings->prestart + 1, &start_game_activate);
}

/**
 * Count the game timer down to 0 then stop the game. We only touch game_time
 * from timer context (except when initializing).
 */
static void game_tick(void) {
	if (game_settings.game_time > 0 && --game_settings.game_time == 0)
		send_game_event(GAME_EVT_STOP_GAME, NULL);
}

/**
 * Activate the player at the beginning of the game. This is where we start the
 * game clock.
 */
static void start_game_activate(void) {
	add_timer(&game_tick, TIMER_HZ, TIMER_RUN_UNLIMITED);
	player_activate();
}

/**
 * Activate a player at the beginning of the game, or after a deactivation
 */
static void player_activate(void) {

	if (!game_state.playing)
		return;

	sound_play_effect(SOUND_POWER_UP);
	game_state.active = 1;

	lights_on();
}

/**
 * Stop the game via a command from IR
 */
static void stop_game_cmd(command_t const *const cmd) {
	stop_game();
}

/**
 * Stop the game.
 */
static void stop_game(void) {
	if (!game_state.playing)
		return;

	lights_off();

	sound_play_effect(SOUND_POWER_DOWN);
	del_timer(&game_tick);
	del_timer(&stun_timer);

	display_write("Game Over");

	//slight issue here: if we stop game during countdown.
	del_timer(&game_countdown);
	game_state.playing = 0;
}

/**
 * Queue __stun_timer.
 */
static void stun_timer(void) {
	send_game_event(GAME_EVT_STUN_TIMER, NULL);
}


/**
 * Timer to run while stunned.
 */
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

/**
 * Handle a shot command from infrared.
 */
static void handle_shot(const uint8_t saddr, command_t const *const cmd) {

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


/**
 * Stun. No error checking is done. game_state should be verified before calling.
 */
static void do_stun(void) {

	game_state.stunned = 1;
	game_countdown_time = game_settings.stun_time;

	lights_stun();

	add_timer(&stun_timer, TIMER_HZ, game_countdown_time);
}

/**
 * Deactivate. No error checking is done. game_state should be verified before calling.
 */
static void do_deac(void) {
	game_state.active = 0;
	lights_off();
	sound_play_effect(SOUND_POWER_DOWN);

	start_countdown(game_settings.deac_time, player_activate);
}

/**
 * Queue a received infrared packet for processing.
 */
static void queue_ir_pkt(const mpc_pkt *const pkt) {
	mpc_incref(pkt);
	send_game_event(GAME_EVT_IR_CMD, pkt);
}

/**
 * Process a queued infrared packet.
 */
static void process_ir_pkt(const mpc_pkt *const pkt) {
	const command_t *const cmd = (command_t *)pkt->data;

	if (cmd->cmd == 0x38) {
		start_game_cmd(cmd);

	} else if (cmd->cmd == 0x08) {
		stop_game_cmd(cmd);

	} else if (cmd->cmd == 0x0c) {
		handle_shot(pkt->saddr, cmd);

		xbee_send(MPC_CMD_SHOT, sizeof(*pkt) + pkt->len, (uint8_t *)pkt);
	}

	mpc_decref(pkt);
}

/**
 * Queue process_trigger()
 */
static void trigger_event(const mpc_pkt *const pkt) {
	send_game_event(GAME_EVT_TRIGGER, NULL);
}

/**
 * Cause the phasor to fire. This is unconditional: The phasor *will* fire.
 */
static void process_trigger(void) {
	static uint8_t data[] = {5, 3, 0xff, 0x0c, 0x63, 0x88, 0xa6};

	if (game_state.active) {
		sound_play_effect(SOUND_LASER);
		mpc_send(MPC_PHASOR_ADDR, MPC_CMD_IR_TX, sizeof(data), data);
	}
}

/**
 * Start a countdown such as the one used for deactivations or game pre-start.
 * The second remaining in the count will be displayed on the screen
 *
 * seconds: The number of seconds.
 * new_countdown_cb: a callback to run when the count complets.
 */
static void start_countdown(const uint8_t seconds, void (*new_countdown_cb)(void)) {
	if (countdown_cb) {
		countdown_cb = NULL;
		del_timer(&game_countdown);
	}

	game_countdown_time = seconds;
	countdown_cb = new_countdown_cb;
	add_timer(&game_countdown, TIMER_HZ, game_countdown_time);
}

static void mpc_hello_received(const mpc_pkt *const pkt) {
	mpc_incref(pkt);
	send_game_event(GAME_EVT_BOARD_HELLO, pkt);
}

static void handle_board_hello(const mpc_pkt *const pkt) {

	if (!gforce_booted && !(boards_booted & pkt->saddr))
		lights_booting(pkt->saddr);

	boards_booted |= pkt->saddr;

	// TODO: send board settings
	uint8_t settings = 0x03;
	mpc_send(pkt->saddr, MPC_CMD_BOARD_CONFIG, sizeof(settings), &settings);

	if (game_state.playing && game_state.active)
		lights_on();

	mpc_decref(pkt);
}
