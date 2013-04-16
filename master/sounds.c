#include <string.h>
#include <avr/pgmspace.h>

#include <stdint.h>

#include "scheduler.h"
#include "sounds.h"

#include <util/delay.h>

#if W_SOUNDS == 1
#include "g4_sounds.h"


sound_player player;

#endif

inline void sound_init() {

    DACB.CTRLA = (DAC_CH0EN_bm | DAC_ENABLE_bm);
    DACB.CTRLB = (DAC_CHSEL_SINGLE_gc);
    DACB.CTRLC = (DAC_REFSEL_AVCC_gc);
    DACB.EVCTRL = 0;


	PORTB.DIRSET |= PIN2_bm | PIN1_bm;

	sound_play_deinit();

#if W_SOUNDS == 1
	memset(&player,0,sizeof player);

#endif
}

inline void sound_play_init() {
#if W_SOUNDS == 1

	DACB.CH0DATA = 0x7FF;

	//change this to unmute
	PORTB.OUTSET |= PIN1_bm;

#endif
}

inline void sound_play_deinit() {


	//MUTE
	PORTB.OUTCLR |= PIN1_bm;

	DACB.CH0DATA = 0x7ff;

}

void sound_play_effect(uint8_t effect) {
#if W_SOUNDS == 1
	player.pos = 0;

	switch (effect) {
		case SOUND_LASER:
			player.effect = sound_laser_fire.effect;
			player.len = pgm_read_word(&sound_laser_fire.len);
			break;
		case SOUND_POWER_UP:
			player.effect = sound_power_up.effect;
			player.len = pgm_read_word(&sound_power_up.len);
			break;
		case SOUND_POWER_DOWN:
			player.effect = sound_power_down.effect;
			player.len = pgm_read_word(&sound_power_down.len);
			break;

		default:
			return;
	}
	sound_play_init();
	scheduler_register(&sound_play_byte,SOUND_SCHEDULER_FREQ, SCHEDULER_RUN_UNLIMITED);
#endif
}

inline void sound_stop(void) {
#if W_SOUNDS == 1
	sound_play_deinit();
	scheduler_unregister(&sound_play_byte);
#endif
}

void sound_play_byte() {
#if W_SOUNDS == 1
//PORTD.OUT ^= PIN5_bm;
	DACB.CH0DATA = (uint16_t)pgm_read_byte(&player.effect[player.pos++]) * 16UL;

	if ( player.pos >= player.len )
		sound_stop();
#endif
}
