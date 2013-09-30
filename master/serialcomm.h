#include <comm.h>
#include <avr/io.h>

#ifndef SERIALCOMM_H
#define SERIALCOMM_H

comm_dev_t * serialcomm_init(register8_t * data, void (*tx_begin)(void), void (*tx_end)(void));
void serialcomm_tx_isr(comm_driver_t * comm);

#endif
