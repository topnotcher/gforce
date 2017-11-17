#include <stdint.h>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#ifndef IRRX_H
#define IRRX_H

void irrx_init(void);

typedef struct __attribute__((packed)) {
	uint8_t cmd;
	uint8_t packs;
	uint8_t passkey;
	uint8_t crc;
} ir_hdr_t;

void ir_rx_simulate(const uint8_t size, const uint8_t *const data);
void ir_rx_recive(uint16_t);
void irrx_hw_init(QueueHandle_t);
void g4_ir_crc(uint8_t *, uint8_t, const uint8_t);

#endif
