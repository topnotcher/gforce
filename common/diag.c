#include <g4config.h>
#include <mpc.h>
#include <diag.h>

static void mpc_reply_ping(const mpc_pkt *const);
static void mpc_echo_ir(const mpc_pkt *const);

void diag_init(void) {
	mpc_register_cmd('P', mpc_reply_ping);
	mpc_register_cmd('I', mpc_echo_ir);
}

/**
 * Used to allow "ping"ing boards - when a P command is received, we reply with
 * an R command.
 */
static void mpc_reply_ping(const mpc_pkt *const pkt) {
	mpc_send_cmd(pkt->saddr, 'R');
}

/**
 * Used to test IR events - when we receive an I command, we send it back to
 * the source as is. This way the master sees the event as if it actually came
 * in via IR.
 */
static void mpc_echo_ir(const mpc_pkt *const pkt) {
	if (pkt->saddr == MPC_MASTER_ADDR)
		mpc_send(pkt->saddr, 'I', pkt->len, (uint8_t *)pkt->data);
}
