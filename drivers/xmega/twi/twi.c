#include "twi.h"

int8_t twi_module_index(const TWI_t *const twi) {
#ifdef TWIC
	if (twi == &TWIC)
		return TWIC_INST_IDX;
#endif
#ifdef TWID
	if (twi == &TWID)
		return TWID_INST_IDX;
#endif
#ifdef TWIE
	if (twi == &TWIE)
		return TWIE_INST_IDX;
#endif
#ifdef TWIF
	if (twi == &TWIF)
		return TWIF_INST_IDX;
#endif

	return -1;
}
