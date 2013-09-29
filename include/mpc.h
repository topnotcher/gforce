#ifndef MPC_H
#define MPC_H

#define MPC_PKT_MAX_SIZE 64

typedef struct {
	uint8_t len;
	uint8_t cmd;
	uint8_t saddr;
	uint8_t chksum;
	uint8_t data [] ;
} mpc_pkt;


void mpc_init(void);

void mpc_send_cmd(const uint8_t addr, const uint8_t cmd);

void mpc_send(const uint8_t addr, const uint8_t cmd, const uint8_t len, uint8_t * const data);

void mpc_rx_process(void);

void mpc_tx_process(void);

void mpc_register_cmd(uint8_t cmd, void (*cb)(const mpc_pkt * const));

#endif
