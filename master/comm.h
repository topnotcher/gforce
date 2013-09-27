#include <stdint.h>

#ifndef COMM_H
#define COMM_H

typedef struct {
	uint8_t daddr;
	uint8_t size;
	uint8_t data[];
} comm_frame_t;

typedef struct {
	comm_frame_t * frame;
	uint8_t byte;
} comm_sender_t;

#endif
