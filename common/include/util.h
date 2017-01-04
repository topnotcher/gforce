#include <stdint.h>
#include <avr/io.h>

#ifndef UTIL_H
#define UTIL_H

//magic macro to concat constants!
#define G4_CONCAT(a,b) a ## b
#define G4_CONCAT3(a,b,c) a##b##c

#define G4_PIN(id) G4_CONCAT3(PIN,id,_bm)
#define G4_PINCTRL(id) G4_CONCAT3(PIN,id,CTRL)

#define CLKSYS_Enable( _oscSel ) ( OSC.CTRL |= (_oscSel) )
#define CLKSYS_IsReady( _oscSel ) ( OSC.STATUS & (_oscSel) )

void crc(uint8_t * const shift, uint8_t byte, const uint8_t poly);
void sysclk_set_internal_32mhz(void); 

#endif
