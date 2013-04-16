
#define bool int
#define false 0
#define true 1

#define lirc_t unsigned int

//#define DEFAULT_FREQ 38000

#include <linux/types.h>

#include <stdint.h>

typedef __u64 ir_code;


#include <sys/ioctl.h>

//#define LIRC_SET_SEND_CARRIER          _IOW('i', 0x00000013, __u32)
//#define LIRC_SET_SEND_DUTY_CYCLE       _IOW('i', 0x00000015, __u32)
//#define LIRC_SET_SEND_MODE             _IOW('i', 0x00000011, __u32)
//#define LIRC_MODE_PULSE      0x00000002
#define LIRC_SET_REC_MODE              _IOW('i', 0x00000012, __u32)

#define LIRC_SETUP_START               _IO('i', 0x00000021)
#define LIRC_SETUP_END                 _IO('i', 0x00000022)


#define WBUF_SIZE (16)

typedef struct {
	uint8_t data[WBUF_SIZE];
	int size;
} cbuf;

#define SYMBOL_SIZE 660

void read_code(int,cbuf *);

int read_timeout(int fd, int * buf, int len, int timeout);
void clear_buf(int);
void process_cmd(cbuf *);

enum ir_sensor_recv_state { 
	ir_state_none,

	//in the data bits.
	ir_state_data,

	//in the parity bit. Well, this bit isn't really
	//used for parity, but whatever... It's the 9th bit.
	ir_state_parity,

	//in the stop bits.
	ir_state_stop,

	ir_state_start
};



