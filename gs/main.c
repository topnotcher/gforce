#include <stdio.h>
#include <sys/socket.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "../include/mpc.h"
#include "../include/util.h"
#include "../include/g4config.h"

#define XBEE_IP "192.168.2.25"
#define XBEE_PORT 9750

uint8_t _crc_ibutton_update(uint8_t crc, uint8_t data);
int mpc_pkt_crc_ok(mpc_pkt *pkt);

int sendtogf(uint8_t cmd, uint8_t *data, uint8_t size);
static void gf_recv(int sock, struct sockaddr_in *xbee_addr);
char *mpc_board_name(const uint8_t board_id);

static char running = 1;

static void handle_cmd(char *cmd);

// The function that'll get passed each line of input
static void my_rlhandler(char *line);
static void my_rlhandler(char *line) {
	if (*line != 0) {
		add_history(line);

		handle_cmd(line);
	}
	free(line);

}

static int sock;
static struct sockaddr_in addr;

int main(int argc, char * *argv) {

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(XBEE_IP);
	addr.sin_port = htons(XBEE_PORT);

	sock = socket(AF_INET, SOCK_DGRAM, 0);

	if (sock < 0) {
		fprintf(stderr, "Error creating socket!\n");
		return 1;
	}

	struct sockaddr_in local;
	memset(&local, 0, sizeof(local));
	local.sin_family = AF_INET;
	local.sin_port = htons(XBEE_PORT);
	local.sin_addr.s_addr = inet_addr("192.168.2.100");
	int flags = fcntl(sock, F_GETFL) | O_NONBLOCK;
	fcntl(sock, F_SETFL, flags);

	bind(sock, (struct sockaddr *)&local, sizeof(local));



	const char *const prompt = "gs> ";

	rl_callback_handler_install(prompt, (rl_vcpfunc_t *)my_rlhandler);

	running = 1;

	while (running) {
		usleep(10000);
		rl_callback_read_char();

		gf_recv(sock, &addr);
	}

	rl_callback_handler_remove();

	return 0;
}

static void handle_cmd(char *cmd) {
	uint8_t start_data[] = {56, 127, 138, 103, 83, 0, 15, 15, 68, 72, 0, 44, 1, 88, 113};
	int start_data_len = 15;

	uint8_t end_data[] = { 8, 127, 153, 250 };
	int end_data_len = 4;

	uint8_t shot_data[] = { 0x0c, 0x63, 0x88, 0xA6 };
	int shot_data_len = 4;

	uint8_t ping_data[] = { MPC_PHASOR_ADDR | MPC_BACK_ADDR | MPC_CHEST_ADDR | MPC_RS_ADDR | MPC_LS_ADDR};
	uint8_t ping_data_len = 1;

	if (!strcmp(cmd, "stop"))
		sendtogf('I', end_data, end_data_len);
	else if (!strcmp(cmd, "start"))
		sendtogf('I', start_data, start_data_len);
	else if (!strcmp(cmd, "shot"))
		sendtogf('I', shot_data, shot_data_len);
	else if (!strcmp(cmd, "ping")) {
		sendtogf('P', ping_data, ping_data_len);
	}
}


int sendtogf(uint8_t cmd, uint8_t *data, uint8_t data_len) {
	mpc_pkt *pkt;
	pkt = malloc(sizeof(*pkt) + data_len + 1) + 1;
	pkt->cmd = cmd;
	pkt->saddr = 4;
	pkt->len = data_len;
	pkt->chksum = MPC_CRC_SHIFT;

	((uint8_t *)pkt)[-1] = 0xff;

	for (uint8_t i = 0; i < sizeof(*pkt) - sizeof(pkt->chksum); ++i)
		pkt->chksum = _crc_ibutton_update(pkt->chksum, ((uint8_t *)pkt)[i]);

	for (uint8_t i = 0; i < pkt->len; ++i) {
		pkt->data[i] = data[i];
		pkt->chksum = _crc_ibutton_update(pkt->chksum, data[i]);
	}

	sendto(sock, (uint8_t *)pkt - 1, sizeof(*pkt) + pkt->len + 1, 0, (struct sockaddr *)
	       &addr, sizeof(addr));

	return 0;

}
static void gf_recv(int sock, struct sockaddr_in *xbee_addr) {
	const uint8_t max_size = 64;
	uint8_t data[max_size];
	int size;
	size = recvfrom(sock, data, max_size, 0, NULL, 0);

	if (size <= 0) return;

	mpc_pkt *pkt = (mpc_pkt *)(data + 1);
	//@TODO compute checksum.
	if (pkt->cmd == 'R') {
		mpc_pkt *reply = (mpc_pkt *)pkt->data;
		char *board = mpc_board_name(reply->saddr);
		char crcok = mpc_pkt_crc_ok(reply) ? '*' : '!';
		printf("PING: reply from [0x%02x:%c] %s\n", reply->saddr, crcok, board);
	} else if (pkt->cmd == 'S') {
		mpc_pkt *shot_data = (mpc_pkt *)pkt->data;
		char *board = mpc_board_name(shot_data->saddr);
		printf("SHOT: %s\n", board);
	} else {
		printf("%c: [0x%02x](%d) ", pkt->cmd, pkt->saddr, pkt->len);
		for (int i = 0; i < pkt->len; ++i)
			printf("0x%02x, ", pkt->data[i]);
		printf("\n");
	}

}
uint8_t _crc_ibutton_update(uint8_t crc, uint8_t data) {
	uint8_t i;

	crc = crc ^ data;
	for (i = 0; i < 8; i++) {
		if (crc & 0x01)
			crc = (crc >> 1) ^ 0x8C;
		else
			crc >>= 1;
	}

	return crc;
}

int mpc_pkt_crc_ok(mpc_pkt *pkt) {
	uint8_t chksum = MPC_CRC_SHIFT;

	for (uint8_t i = 0; i < sizeof(*pkt) - sizeof(pkt->chksum); ++i)
		chksum = _crc_ibutton_update(chksum, ((uint8_t *)pkt)[i]);

	for (uint8_t i = 0; i < pkt->len; ++i) {
		chksum = _crc_ibutton_update(chksum, pkt->data[i]);
	}

	return chksum == pkt->chksum;
}

char *mpc_board_name(const uint8_t board_id) {
	switch (board_id) {
	case MPC_CHEST_ADDR:
		return "chest";
	case MPC_LS_ADDR:
		return "left shoulder";
	case MPC_BACK_ADDR:
		return "back";
	case MPC_RS_ADDR:
		return "right shoulder";
	case MPC_MASTER_ADDR:
		return "master";
	case MPC_PHASOR_ADDR:
		return "phasor";
	default:
		return "???";
	}
}
