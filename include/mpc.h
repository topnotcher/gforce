#ifndef MPC_H
#define MPC_H

typedef struct {

	struct { 
		uint8_t cmd;
		uint8_t len;
		uint8_t saddr;

	} hdr;

	uint8_t chksum;

	uint8_t * data ;

} mpc_pkt;



void mpc_init(void);

void mpc_send_cmd(const uint8_t addr, const uint8_t cmd);

void mpc_send(const uint8_t addr, const uint8_t cmd, uint8_t * const data, const uint8_t len);

mpc_pkt * mpc_recv(void);
#endif
