#include <stdio.h>
#include <sys/socket.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <stdlib.h>
#include "../include/mpc.h"
#include "../include/util.h"
#include "../include/g4config.h"

#define XBEE_IP "192.168.2.25"
#define XBEE_PORT 9750

uint8_t _crc_ibutton_update(uint8_t crc, uint8_t data);

int sendtogf(uint8_t cmd,uint8_t * data, uint8_t size);
static void gf_recv(int sock, struct sockaddr_in * xbee_addr);


int main(int argc, char**argv) {

	uint8_t start_data[] = {56, 127 ,138,103,83,0,15,15,68,72,0,44,1,88,113};
	int start_data_len = 15;

	uint8_t end_data[] = { 8, 127, 153, 250 };
	int end_data_len = 4;

	uint8_t shot_data[] = { 0x0c, 0x63, 0x88, 0xA6 };
	int shot_data_len = 4;

	uint8_t ping_data[] = { MPC_BACK_ADDR | MPC_CHEST_ADDR | MPC_RS_ADDR | MPC_LS_ADDR};
	uint8_t ping_data_len = 1;

	if ( argc < 2 ) {
		fprintf(stderr, "Usage: %s start|stop|shot\n",argv[0]);
		return 1;
	}

	if (!strcmp(argv[1],"stop")) 
		sendtogf('I',end_data,end_data_len);
	else if (!strcmp(argv[1], "start"))
		sendtogf('I',start_data,start_data_len);
	else if (!strcmp(argv[1],"shot"))
		sendtogf('I',shot_data,shot_data_len);
	else if (!strcmp(argv[1], "ping")) {
		sendtogf('P',ping_data, ping_data_len);
	}

	return 0;
}


int sendtogf(uint8_t cmd, uint8_t * data, uint8_t data_len) {
	int sock;
	struct sockaddr_in addr;
	
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(XBEE_IP);
	addr.sin_port = htons(XBEE_PORT);	

	sock = socket(AF_INET, SOCK_DGRAM, 0);

	if ( sock < 0 ) {
		fprintf(stderr,"Error creating socket!\n");
		return 1;
	}

	struct sockaddr_in local;
	memset(&local, 0, sizeof(local));
	local.sin_family = AF_INET;
	local.sin_port = htons(XBEE_PORT);
	local.sin_addr.s_addr = inet_addr("192.168.2.100");
	bind(sock, (struct sockaddr *)&local, sizeof(local));

	mpc_pkt * pkt;
	pkt = malloc(sizeof(*pkt)+data_len+1)+1;
	pkt->cmd = cmd;
	pkt->saddr = 4;
	pkt->len = data_len;
	pkt->chksum = MPC_CRC_SHIFT;

	((uint8_t*)pkt)[-1] = 0xff;

	for ( uint8_t i = 0; i < sizeof(*pkt)-sizeof(pkt->chksum); ++i )
		pkt->chksum = _crc_ibutton_update(pkt->chksum, ((uint8_t*)pkt)[i]);

	for ( uint8_t i = 0; i < pkt->len; ++i ) {
		pkt->data[i] = data[i];
		pkt->chksum = _crc_ibutton_update(pkt->chksum, data[i]);
	}

	sendto(sock, (uint8_t*)pkt-1,sizeof(*pkt)+pkt->len+1, 0, (struct sockaddr *)
               &addr, sizeof(addr));


	gf_recv(sock, &addr);

	return 0;

}
static void gf_recv(int sock, struct sockaddr_in * xbee_addr) {
	const uint8_t max_size = 64;
	uint8_t data[max_size];
	uint8_t size;
	socklen_t * fromsize = (socklen_t*)sizeof(*xbee_addr);
	while (1) {
		size = recvfrom(sock,data,max_size,0,(struct sockaddr*)xbee_addr, fromsize);
		
		if ( size <= 0 ) continue;

		mpc_pkt * pkt = (mpc_pkt*)(data+1);
		
		if ( pkt->cmd == 'R' ) {
			mpc_pkt * reply = (mpc_pkt*)pkt->data;
			char * board;
			switch(reply->saddr) {
			case MPC_CHEST_ADDR:
				board = "chest";
				break;
			case MPC_LS_ADDR:
				board = "left shoulder";
				break;
			case MPC_BACK_ADDR:
				board = "back";
				break;
			case MPC_RS_ADDR:
				board = "right shoulder";
				break;
			default:
				board = "???";
				break;
			}
			
			printf("PING: reply from [0x%02x] %s\n",reply->saddr, board);

		} else { 
			printf("%c: [0x%02x](%d) ", pkt->cmd, pkt->saddr, pkt->len);
			for (int i = 0; i < pkt->len; ++i)
				printf("0x%02x, ", pkt->data[i]);
			printf("\n");
		} 
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
