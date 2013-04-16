#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include "irdebug.h"
#include "../util.h"

int main() {

	int fd = open("/dev/lirc0", O_RDWR);

	cbuf buf;

	memset(&buf, 0, sizeof(buf) );

	while ( true ) 
		read_code(fd, &buf);

	close(fd);

	return 0;
}

/**
 * See: http://www.lirc.org/html/technical.html
 * This shit is a pain in the balls
 */
inline void read_code(int fd, cbuf * buf) {

	//number of bits we've shifted into the current byte.
	static int bits = 0;

	//value of the parity bit.
	static int parity = 0;

	//haz we been cleaned up by a break yet???
	static int dirty = 1;

	static enum ir_sensor_recv_state state = ir_state_none;

	lirc_t data = 0;

	int size = read_timeout(fd,&data ,sizeof(data),20*SYMBOL_SIZE);
		
	//timeout or error.
	if (size <= 0) {
		if ( buf->size ) {
			printf("Process command due to timeout\n");
			process_cmd( buf );
		}

		dirty = 1;
		buf->size = 0;
		return;
	}

	int type = data>>24;
	int pulse = data&0xFFFFFF;
	int bit = type^1;
	
	/*if ( bit )
		printf("space %d\n", pulse);
	else
		printf("pulse %d\n", pulse);*/
	

	//now we pretend like we're reading serial data.
	//This is essentially a 'foreach bit' loop.
	//Setting the error threshold too low will fuck shit up.
	while ( pulse >= (.7*SYMBOL_SIZE) ) {
		pulse -= SYMBOL_SIZE;
			
		switch ( state ) {
				
		/**
		 * Idle
		 */
		case ir_state_none:

			//this is a fucking start bit.
			if ( bit == 0 ) {
				state = ir_state_data;
				bits = 0;

//				printf("--- start frame --- \n");
				buf->data[buf->size] = 0;
			}
					
			break; /* ir_state_none */

		case ir_state_data:
			if ( bit ) {
				buf->data[buf->size] |= 1<<bits;
//				printf("\t byte |= 1<<%d\n",bits);
			}

			///done read all the bits!
			if ( ++bits == 8 ) {
				state = ir_state_parity;
				buf->size++;
			}

			break; /* ir_state_data */

		case ir_state_parity:
			parity = bit;
			state = ir_state_stop;
			break; /* ir_state parity */


		case ir_state_stop:
				
			//no matter what happens, we set the state to none 
			//and wait for the next start bit.
				
			state = ir_state_none;
		
//			printf("* byte: %d\n", buf->data[buf->size-1] );
//			printf(" --- end frame ---\n");
			//don't do anything special if we're high during the end.
			if ( !bit ) {
				printf("Why is the line low during  a  stop???\n");
			//	dirty = 1;continue;
			}

			// give me a break
			if ( !parity && buf->data[buf->size-1] == 0xFF ) {
				
				//got a break, but we have data in the buffer.
				//Lets assume the data is a command before we flush it.
				if ( buf->size >= 5 ) {
					buf->size--;
					process_cmd(buf);
					printf("Process command due to break\n");

				}

				dirty = 0;
				buf->size = 0;
				
				// regardless of anything else, if we get a break,
				// we know whatever is in the buffer is done.

			// I'm so stanky.
			} else if ( dirty ) {
				buf->size = 0;
			
			//tooo much damn data!
			} else if ( buf->size == WBUF_SIZE ) {
				process_cmd( buf );
				printf("Process command due to full buffer\n");

				dirty = 1;
				buf->size = 0;
			}


			break; /* ir_state_stop */
		}
	}
}


void process_cmd( cbuf * buf ) {

	printf("\n--------------------------------------------\n= command: %d bytes: =\n",buf->size);
	uint8_t shift = 0x55;
	uint8_t poly = 0x24;

	for ( int i = 0; i < buf->size; ++i ) {

		if ( i == 3 || i == buf->size-1 ) 
			printf("%2d: 0x%02x (crc: 0x%02x)\n",  i, buf->data[i], shift);
		else {
			crc(&shift, buf->data[i], poly);
			printf("%2d: 0x%02x",  i, buf->data[i]);

			if ( i == 0 )
				printf(" (cmd)");
			if ( i == 1 )
				printf(" (packs)");

			printf("\n");
		}
	}
}

int read_timeout(int fd, int *buf, int len, int timeout) {

	fd_set fds;
	struct timeval tv;
	int ret, n;

	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	tv.tv_sec = 0;
	tv.tv_usec = timeout;

	do {
		ret = select(fd+1, &fds, NULL, NULL, &tv);
	}
	while (ret == -1 /*&& errno == EINTR*/);

	if (ret == -1) {
		return (-1);
	} else if (ret == 0)
		return (0); /* timeout */

	n = read(fd, buf, len);

	if (n == -1) {
		return (-1);
	}

	return (n);
}

void clear_buf(int fd) {
	int size;
	unsigned int buf = 0;

	do {
		size = read_timeout(fd, &buf, sizeof(buf), 1);
	} while (size > 0);

}
