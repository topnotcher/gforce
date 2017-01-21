#include <g4config.h>
#include <malloc.h>
#include <mpc.h>
#include <diag.h>
#include "irrx.h"

static void mpc_reply_ping(const mpc_pkt *const);
static void mpc_echo_ir(const mpc_pkt *const);
static void mpc_mem_usage(const mpc_pkt *const pkt);

void diag_init(void) {
	mpc_register_cmd(MPC_CMD_DIAG_PING, mpc_reply_ping);
	mpc_register_cmd(MPC_CMD_IR_RX, mpc_echo_ir);
	mpc_register_cmd(MPC_CMD_DIAG_MEM_USAGE, mpc_mem_usage);
}

/**
 * Used to allow "ping"ing boards - when a P command is received, we reply with
 * an R command.
 */
static void mpc_reply_ping(const mpc_pkt *const pkt) {
	mpc_send_cmd(pkt->saddr, MPC_CMD_DIAG_RELAY);
}

/**
 * Used to test IR events - when we receive an I command, we send it to the
 * IR receiver task to be processed in the exact same way as incoming IR.
 */
static void mpc_echo_ir(const mpc_pkt *const pkt) {
	if (pkt->saddr == MPC_ADDR_MASTER)
		ir_rx_simulate(pkt->len, pkt->data);
}

static void mpc_mem_usage(const mpc_pkt *const pkt) {
	mem_usage_t usage = mem_usage();

	if (pkt->saddr == MPC_ADDR_MASTER)
		mpc_send(pkt->saddr, MPC_CMD_DIAG_MEM_USAGE, sizeof(usage), (uint8_t*)&usage);
}

// TODO: this is not implemented on SAML21 yet
void __attribute__((weak)) ir_rx_simulate(const uint8_t size, const uint8_t *buf) {}
