#include <stdint.h>

#ifndef TWI_MASTER_H
#define TWI_MASTER_H

#define TWI_MASTER_STATUS_UNKNOWN 1
#define TWI_MASTER_STATUS_OK 0
#define TWI_MASTER_STATUS_ABBR_LOST 2
#define TWI_MASTER_STATUS_SLAVE_NAK 3

struct _twi_master_s;
typedef struct _twi_master_s twi_master_t;

typedef void (*twi_master_complete_txn_cb)(void *, int8_t);

void twi_master_write_read(twi_master_t *, uint8_t, uint8_t, uint8_t *, uint8_t, uint8_t *);
void twi_master_write(twi_master_t *, uint8_t, uint8_t, uint8_t *); 
void twi_master_read(twi_master_t *, uint8_t, uint8_t, uint8_t *); 
void twi_master_set_blocking(twi_master_t *, void (*)(void), void (*)(void));
void twi_master_set_callback(twi_master_t *, void *, twi_master_complete_txn_cb);
#endif
