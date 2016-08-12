#ifndef DISPLAYCOMM_H
#define DISPLAYCOMM_H

//2x16+one new line+\n?
#define DISPLAY_PKT_MAX_SIZE 18
typedef struct {
	uint8_t cmd;
	uint8_t size;
	uint8_t data[];
} display_pkt;

#endif
