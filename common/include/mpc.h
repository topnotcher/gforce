#include "mpc_cmds.h"
#include "mempool.h"

#ifndef MPC_H
#define MPC_H

#define MPC_PKT_MAX_SIZE 64

typedef struct {
	uint8_t len;
	uint8_t cmd;
	uint8_t saddr;
	uint8_t chksum;
	uint8_t data [] ;
} __attribute__((packed)) mpc_pkt;


void mpc_init(void);
void mpc_send_cmd(const uint8_t addr, const uint8_t cmd);
void mpc_send(const uint8_t addr, const uint8_t cmd, const uint8_t len, uint8_t * const data);
void mpc_register_cmd(const uint8_t cmd, void (*cb)(const mpc_pkt * const));

static inline void mpc_decref(const mpc_pkt *const pkt) {
	mempool_putref((void*)pkt);
}

static inline void mpc_incref(const mpc_pkt *const pkt) {
	mempool_getref((void*)pkt);
}
#endif
