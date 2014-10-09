#include <string.h>
#include <avr/pgmspace.h>

#include <stdint.h>

#include "timer.h"
#include "sounds.h"

#include <util/delay.h>

#if W_SOUNDS == 1
#include "g4_sounds.h"

#define tick_enable() do { TCC0.CNT = 0; TCC0.INTCTRLB |= TC_CCAINTLVL_HI_gc; } while(0)
#define tick_disable() TCC0.INTCTRLB &= ~TC_CCAINTLVL_HI_gc


sound_player player;

#endif

inline void sound_init() {
	
	TCC0.CCA = 63;
	TCC0.CTRLB |= TC0_CCAEN_bm;
	
	TCC0.CTRLA = TC_CLKSEL_DIV64_gc;


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
	PORTB.OUTSET = PIN1_bm;
	
	tick_enable();
#endif
}

inline void sound_play_deinit() {


	//MUTE
	PORTB.OUTCLR = PIN1_bm;

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
#endif
}

inline void sound_stop(void) {
#if W_SOUNDS == 1
	tick_disable();
	sound_play_deinit();
//	timer_unregister(&sound_play_byte);
#endif
}

void sound_play_byte() {
#if W_SOUNDS == 1
//PORTD.OUT ^= PIN5_bm;
	DACB.CH0DATA = (uint16_t)pgm_read_byte(&player.effect[player.pos++]) * 16UL;
	TCC0.CNT = 0;
	if ( player.pos >= player.len )
		sound_stop();
#endif
}

ISR(TCC0_CCA_vect) {
	sound_play_byte();
}
