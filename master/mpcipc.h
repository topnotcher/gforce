#include <mpc.h>
#include <stdint.h>

mpc_driver_t *mpc_loopback_init(uint8_t, uint8_t);
void mpc_loopback_send (mpc_driver_t *, const uint8_t, const uint8_t, uint8_t *const);
