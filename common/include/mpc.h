#include "comm.h"
#include "mpc_cmds.h"

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
void mpc_rx_xbee(uint8_t, uint8_t*);



// mpc_pkt *pkt = (comm_frame_t*)->data; This macro gets us the address of the
// comm_frame_t which is reference counted by mempool allocator.
#define __mpc_pkt_buf(pkt) ((void*)(&((uint8_t*)pkt)[-1 * offsetof(comm_frame_t, data)]))

static inline void mpc_decref(const mpc_pkt *const pkt) {
	mempool_putref(__mpc_pkt_buf(pkt));
}

static inline void mpc_incref(const mpc_pkt *const pkt) {
	mempool_getref(__mpc_pkt_buf(pkt));
}
#endif
