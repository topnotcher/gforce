#include <stdint.h>

#ifndef TWI_SLAVE_H
#define TWI_SLAVE_H

struct _twi_slave_s;
typedef struct _twi_slave_s twi_slave_t;

typedef void (*twi_slave_begin_txn)(void *, uint8_t, uint8_t **, uint8_t *);
typedef void (*twi_slave_end_txn)(void *, uint8_t, uint8_t *, uint8_t);

void twi_slave_set_callbacks(twi_slave_t *, void *,  twi_slave_begin_txn, twi_slave_end_txn);
#endif
