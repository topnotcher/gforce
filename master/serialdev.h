#include <comm.h>
#include <avr/io.h>

#ifndef SERIALDEV_H
#define SERIALDEV_H

comm_dev_t * serialdev_init(register8_t * data, void (*tx_begin)(void), void (*tx_end)(void));
void serialdev_tx_isr(comm_driver_t * comm);

#endif
