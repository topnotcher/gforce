#include <stdint.h>
#include <stdbool.h>

#include "mpc_cmds.h"
#include "mempool.h"

#include "FreeRTOS.h"
#include "queue.h"

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

struct __mpc_driver;
typedef struct __mpc_driver {
	// interface address
	uint8_t addr;

	// destination address mask
	uint8_t addrmask;

	// mpc_pkt transmit callback
	void (*tx)(struct __mpc_driver *, const uint8_t, const uint8_t, uint8_t *const);

	// called by mpc monostate when driver is registered.
	bool (*registered)(struct __mpc_driver *);
	void *ins;

	void (*tx_complete)(void *);
	uint8_t *(*alloc_buf)(uint8_t *const);
	QueueHandle_t *rx_queue;
} mpc_driver_t;

struct mpc_rx_data {
	uint8_t size;
	uint8_t *buf;
};

void mpc_init(void);
bool mpc_register_driver(mpc_driver_t *);
void mpc_send_cmd(const uint8_t addr, const uint8_t cmd);
void mpc_send(const uint8_t addr, const uint8_t cmd, const uint8_t len, uint8_t * const data);
void mpc_register_cmd(const uint8_t cmd, void (*cb)(const mpc_pkt * const));

inline void mpc_decref(const mpc_pkt *const pkt) {
	mempool_putref((void*)pkt);
}

inline void mpc_incref(const mpc_pkt *const pkt) {
	mempool_getref((void*)pkt);
}
#endif
