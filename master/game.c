#include <stdint.h>
#include <avr/io.h>

#include <mpc.h>

#include "display.h"
#include "sounds.h"
#include "game.h"
#include "lights.h"
#include "xbee.h"

#include <g4config.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/timers.h>

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

const uint8_t all_boards = MPC_ADDR_LS | MPC_ADDR_RS | MPC_ADDR_CHEST | MPC_ADDR_BACK | MPC_ADDR_PHASOR;
static uint8_t boards_booted;
static uint8_t gforce_booted;

static TimerHandle_t countdown_timer;
static TimerHandle_t stun_timer;
static TimerHandle_t game_timer;

static void game_task(void *);
static void process_game_event(game_event *evt);

static void process_trigger(void);
static void trigger_event(const mpc_pkt *const);
static void process_ir_pkt(const mpc_pkt *const);
static void queue_ir_pkt(const mpc_pkt *const);

static void start_countdown(const uint8_t, void (*)(void));
static void game_countdown(TimerHandle_t);
static void __game_countdown(void);
static void stun_tick(TimerHandle_t);
static void __stun_tick(void);

static void handle_shot(const mpc_pkt *const);
static void do_stun(void);
static void do_deac(void);
static void start_game_cmd(command_t const *const);
static void stop_game_cmd(command_t const *const);
static void stop_game(void);

static void game_tick(TimerHandle_t);
static void start_game_activate(void);
static void player_activate(void);

static void gforce_boot_tick(TimerHandle_t);
static void _gforce_boot_tick(void);
static void mpc_hello_received(const mpc_pkt *const);
static void handle_board_hello(const mpc_pkt *const);


void game_init(void) {
	game_evt_queue = xQueueCreate(8, sizeof(game_event));
	xTaskCreate(game_task, "game", 384, game_evt_queue, tskIDLE_PRIORITY + 6, (TaskHandle_t*)NULL);
}

static void game_task(void *params) {
	QueueHandle_t evt_queue = params;
	game_event evt;

	game_state.playing = 0;
	game_state.active = 0;
	game_state.stunned = 0;

	mpc_register_cmd(MPC_CMD_HELLO, mpc_hello_received);

	game_timer = xTimerCreate("game tick", configTICK_RATE_HZ, 1, NULL, game_tick); 
	stun_timer = xTimerCreate("stun tick", configTICK_RATE_HZ, 1, NULL, stun_tick); 

	/**
	 * Add a 10 second timer to wait for all boards to come up.
	 */
	countdown_timer = xTimerCreate("boot tick", configTICK_RATE_HZ, 1, NULL, gforce_boot_tick); 
	xTimerStart(countdown_timer, portMAX_DELAY);

	while (1) {
		if (xQueueReceive(evt_queue, &evt, portMAX_DELAY)) {
			process_game_event(&evt);
		}
	}
}

static void gforce_boot_tick(TimerHandle_t _) {
	send_game_event(GAME_EVT_BOOT_TICK, NULL);
}

static void _gforce_boot_tick(void) {
	static uint8_t ticks = GFORCE_BOOT_TICKS;

	if (gforce_booted)
		return;

	ticks--;

	// timeout when the tick count  expires
	// but allow boot complete at GFORCE_BOOT_TICKS/2
	if (ticks == 0 || (ticks < GFORCE_BOOT_TICKS/2 && boards_booted == all_boards)) {
		xTimerDelete(countdown_timer, portMAX_DELAY);
		countdown_timer = xTimerCreate("game countdown", configTICK_RATE_HZ, 1, NULL, game_countdown); 

		mpc_register_cmd(MPC_CMD_IR_RX, queue_ir_pkt);
		mpc_register_cmd(MPC_CMD_IR_TX, trigger_event);

		if (boards_booted == all_boards)
			display_write("GForce Booted");
		else
			display_write("Error: \nBoards missing!");

		lights_off();

		xbee_send(MPC_CMD_HELLO, 0, NULL);

		gforce_booted = 1;

	} else {
		if (all_boards ^ boards_booted)
			mpc_send_cmd(all_boards ^ boards_booted, MPC_CMD_HELLO);

		display_write("Booting...      ");
	}
}

static void process_game_event(game_event *evt) {
	switch (evt->type) {
	case GAME_EVT_COUNTDOWN:
		__game_countdown();
		break;

	case GAME_EVT_STUN_TIMER:
		__stun_tick();
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
		xbee_send(MPC_CMD_MEMBER_LOGIN, 8, evt->data);
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

static void game_countdown(TimerHandle_t _) {
	send_game_event(GAME_EVT_COUNTDOWN, NULL);
}

static void __game_countdown(void) {
	static uint8_t data[] = "        ";

	if (game_countdown_time == 0 || --game_countdown_time == 0) {
		xTimerStop(countdown_timer, portMAX_DELAY);

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

	uint8_t stun = 4 - settings->stuns;

	if (stun)
		game_settings.stun_time = game_settings.deac_time / stun;
	else
		game_settings.stun_time = 0;

	start_countdown(settings->prestart + 1, &start_game_activate);
}

/**
 * Count the game timer down to 0 then stop the game. We only touch game_time
 * from timer context (except when initializing).
 */
static void game_tick(TimerHandle_t _) {
	if (game_settings.game_time > 0 && --game_settings.game_time == 0)
		send_game_event(GAME_EVT_STOP_GAME, NULL);
}

/**
 * Activate the player at the beginning of the game. This is where we start the
 * game clock.
 */
static void start_game_activate(void) {
	xTimerStart(game_timer, portMAX_DELAY);
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
	xTimerStop(game_timer, portMAX_DELAY);
	xTimerStop(stun_timer, portMAX_DELAY);

	display_write("Game Over");

	//slight issue here: if we stop game during countdown.
	xTimerStop(countdown_timer, portMAX_DELAY);
	game_state.playing = 0;
}

/**
 * Queue __stun_timer.
 */
static void stun_tick(TimerHandle_t _) {
	send_game_event(GAME_EVT_STUN_TIMER, NULL);
}


/**
 * Timer to run while stunned.
 */
static void __stun_tick(void) {

	if (!game_state.playing) {
		xTimerStop(stun_timer, portMAX_DELAY);
		return;
	}

	if (game_countdown_time > 0)
		--game_countdown_time;

	if (game_countdown_time == 0) {
		lights_unstun();
		game_state.stunned = 0;
		game_state.active = 1;
		xTimerStop(stun_timer, portMAX_DELAY);
		display_send(0, 2, (uint8_t *)" ");

	} else if (game_countdown_time == (game_settings.stun_time >> 1)) {
		lights_halfstun();
	}
}

/**
 * Handle a shot command from infrared.
 */
static void handle_shot(const mpc_pkt *const pkt) {

	if (!game_state.active || !game_state.playing || game_state.stunned)
		return;

	xbee_send_pkt(pkt);

	char *sensor;
	switch (pkt->saddr) {
	case MPC_ADDR_CHEST:
		sensor = "Tagged: CH";
		break;
	case MPC_ADDR_LS:
		sensor = "Tagged: LS";
		break;
	case MPC_ADDR_BACK:
		sensor = "Tagged: BA";
		break;
	case MPC_ADDR_RS:
		sensor = "Tagged: RS";
		break;
	case MPC_ADDR_PHASOR:
		sensor = "Tagged: PH";
		break;
	default:
		sensor = "Tagged: ??";
		break;
	}

	// tagged in back or chest, or stuns disabled.
	if ((pkt->saddr & (MPC_ADDR_BACK | MPC_ADDR_CHEST)) || !game_settings.stun_time)
		do_deac();
	else
		do_stun();

	display_write(sensor);
}


/**
 * Stun. No error checking is done. game_state should be verified before calling.
 */
static void do_stun(void) {

	game_state.stunned = 1;
	game_countdown_time = game_settings.stun_time;

	lights_stun();

	xTimerStart(stun_timer, portMAX_DELAY);
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

	switch (cmd->cmd) {

	case IR_CMD_GAME_START:
		start_game_cmd(cmd);
		break;

	case IR_CMD_GAME_STOP:
		stop_game_cmd(cmd);
		break;

	case IR_CMD_SHOT:
		handle_shot(pkt);
		break;

	default:
		break;
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
		mpc_send(MPC_ADDR_PHASOR, MPC_CMD_IR_TX, sizeof(data), data);
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
	game_countdown_time = seconds;
	countdown_cb = new_countdown_cb;

	xTimerStart(countdown_timer, portMAX_DELAY);
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
	mpc_send(pkt->saddr, MPC_CMD_CONFIG, sizeof(settings), &settings);

	if (game_state.playing && game_state.active)
		lights_on();

	mpc_decref(pkt);
}
