#include <stdint.h>
#include <drivers/sam/dma.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "../fatfs/ff.h"
#include "mp3/mp3dec.h"

#ifndef SOUNDS_H
#define SOUNDS_H



#define SOUND_LASER "snd_019.mp3"
#define SOUND_PRE_GAME_START "snd_002.mp3"
#define SOUND_POWER_UP "snd_004.mp3"
#define SOUND_STUN "snd_007.mp3"
#define SOUND_POWER_DOWN "snd_005.mp3"
#define SOUND_GAME_END "snd_003.mp3"

#define SOUND_NUM_OUT_BUFS 4
#define SOUND_OUT_BUF_SIZE 1152
#define SOUND_IN_BUF_SIZE 4096
#define SOUND_IN_BUF_LOW_THRESHOLD 1024

typedef int16_t mp3_data_t;

struct mp3outbuf {
	int size;
	mp3_data_t data[SOUND_OUT_BUF_SIZE];
};

struct mp3inbuf {
	uint8_t *read_ptr;
	int size;
	uint8_t data[SOUND_IN_BUF_SIZE];
};

typedef struct { 
	DmacDescriptor linked_descs[SOUND_NUM_OUT_BUFS - 1];
	FIL *fp;
	char *filename;
	TaskHandle_t task;
	DmacDescriptor *dma[SOUND_NUM_OUT_BUFS];
	MP3FrameInfo frame_info;
	HMP3Decoder decoder;
	struct mp3outbuf outbufs[SOUND_NUM_OUT_BUFS];
	struct mp3inbuf inbuf;
	int8_t dma_chan;
	uint8_t cur_buf;
	uint8_t bufs_full;

} sound_player;

void sound_init(void);
void sound_play_effect(typeof(SOUND_LASER));
#endif
