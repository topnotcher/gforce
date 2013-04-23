#include <stdint.h>
#include <util.h>

void crc(uint8_t * const shift, uint8_t byte, const uint8_t poly) {
	uint8_t fb;

	for ( uint8_t i = 0; i < 8; i++) {
		fb = (*shift ^ byte) & 0x01; 

		if ( fb )
			*shift = 0x80|(((*shift) ^ poly)>>1);
		else 
			*shift >>= 1;
		byte >>= 1;
	}
}
