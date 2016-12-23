#include <comm.h>
#include <avr/io.h>

#ifndef SERIALCOMM_H
#define SERIALCOMM_H

typedef struct {
	void *dev;
	uint8_t (*rx_func)(void *);
	void (*tx_func)(void *, uint8_t *, uint8_t, void (*)(void *));
} serialcomm_driver;

comm_dev_t *serialcomm_init(const serialcomm_driver, uint8_t);
void serialcomm_send(comm_driver_t *, uint8_t, const uint8_t, uint8_t *, void (*)(void *));
#endif
