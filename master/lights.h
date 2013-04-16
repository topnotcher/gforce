#include "ports.h"

#define LIGHTS_PORTID B
#define LIGHTS_ENABLE 0
#define LIGHTS_STUN 1


#define LIGHTS_PORT PORT_ID(LIGHTS_PORTID)
#define LIGHTS_DDR DDR_ID(LIGHTS_PORTID)

//enable lights
//I'm breaking case convention in case i decide to make functions out of these.
#define lights_on() LIGHTS_PORT |= _BV(LIGHTS_ENABLE)
#define lights_off() LIGHTS_PORT &= ~_BV(LIGHTS_ENABLE)

//enable stun lights
#define lights_stun() LIGHTS_PORT |= _BV(LIGHTS_STUN)
#define lights_unstun() LIGHTS_PORT &= ~_BV(LIGHTS_STUN)

inline void lights_init(void) { 
	
	LIGHTS_DDR |= _BV(LIGHTS_ENABLE) | _BV(LIGHTS_STUN);

	lights_off();
	lights_unstun();
}
