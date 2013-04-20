#include <stdint.h>
#include <util.h>

void crc(uint8_t * const shift, uint8_t byte, const uint8_t poly) {
	//byte &= 0xff;
	uint8_t fb;

	for ( uint8_t i = 0; i < 8; i++) {
		fb = (*shift ^ byte) & 0x01; // one bit

		if ( fb )
			*shift = (*shift) ^ poly;

		*shift >>= 1;
		*shift = ( (*shift) | (fb << 7) );
		byte >>= 1;
	}
}

