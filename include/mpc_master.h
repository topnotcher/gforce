#ifndef MPC_MASTER_H
#define MPC_MASTER_H

void mpc_master_init(void);


//send a command with 0 data lenght
void mpc_master_send_cmd(const uint8_t addr, const uint8_t cmd);

//send whatever.
void mpc_master_send(const uint8_t addr, const uint8_t cmd, uint8_t * const data, const uint8_t len);


//check if there is any data to send. If there is, configure the sender magig
void mpc_master_run(void);


#endif
