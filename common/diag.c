#include <g4config.h>
#include <malloc.h>
#include <mpc.h>
#include <diag.h>
#include "irrx.h"

static void mpc_reply_ping(const mpc_pkt *const);
static void mpc_echo_ir(const mpc_pkt *const);
static void mpc_mem_usage(const mpc_pkt *const pkt);

void diag_init(void) {
	mpc_register_cmd('P', mpc_reply_ping);
	mpc_register_cmd('I', mpc_echo_ir);
	mpc_register_cmd('M', mpc_mem_usage);
}

/**
 * Used to allow "ping"ing boards - when a P command is received, we reply with
 * an R command.
 */
static void mpc_reply_ping(const mpc_pkt *const pkt) {
	mpc_send_cmd(pkt->saddr, 'R');
}

/**
 * Used to test IR events - when we receive an I command, we send it to the
 * IR receiver task to be processed in the exact same way as incoming IR.
 */
static void mpc_echo_ir(const mpc_pkt *const pkt) {
	if (pkt->saddr == MPC_MASTER_ADDR)
		ir_rx_simulate(pkt->len, pkt->data);
}

static void mpc_mem_usage(const mpc_pkt *const pkt) {
	mem_usage_t usage = mem_usage();

	if (pkt->saddr == MPC_MASTER_ADDR)
		mpc_send(pkt->saddr, 'M', sizeof(usage), (uint8_t*)&usage);
}
