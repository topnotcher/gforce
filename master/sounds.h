#include <stdint.h>

#ifndef SOUNDS_H
#define SOUNDS_H



#define SOUND_LASER 0
#define SOUND_POWER_UP 1
#define SOUND_POWER_DOWN 2

typedef struct { 
	uint8_t const *effect;
	uint16_t len;
	uint16_t pos;
} sound_player;

typedef struct {
	const uint16_t len;
	const uint8_t *effect;

} sound_effect;

void sound_init(void);
void sound_play_byte(void);
void sound_play_effect(uint8_t);
void sound_play_init(void);
void sound_play_deinit(void);
#endif
