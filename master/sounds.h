#include <stdint.h>

#ifndef SOUNDS_H
#define SOUNDS_H


#define SOUND_OUTPUT_PIN 4
#define SOUND_OUTPUT TCD0.CCA


#define SOUND_CONTROL_REGISTER_A TCD0.CTRLA
#define SOUND_CONTROL_REGISTER_B TCD0.CTRLB

#define SOUND_CONTROL_REGISTER_A_BITS TC_CLKSEL_DIV1_gc

//TC0_CCAEN_bm might nto be right. Is this a T0 or a T1? FML.
#define SOUND_CONTROL_REGISTER_B_BITS TC0_CCAEN_bm | TC_WGMODE_SS_gc

//idfk
#define SOUND_SCHEDULER_FREQ 16

#define SOUND_LASER 0
#define SOUND_POWER_UP 1
#define SOUND_POWER_DOWN 2

typedef struct { 
	uint8_t const * effect;
	uint16_t len;
	uint16_t pos;
} sound_player;

typedef struct {
	const uint16_t len;
	const uint8_t effect[];

} sound_effect;

void sound_init(void);
void sound_play_byte(void);
void sound_play_effect(uint8_t);
void sound_play_init(void);
void sound_play_deinit(void);
#endif
