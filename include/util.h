#include <stdint.h>

#ifndef UTIL_H
#define UTIL_H

//magic macro to concat constants!
#define G4_CONCAT(a,b) a ## b


void crc(uint8_t * const shift, uint8_t byte, const uint8_t poly);


#endif
