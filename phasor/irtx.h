#ifndef IR_TX_H
#define IR_TX_H

void irtx_init(void);

void irtx_send(uint16_t data[], uint8_t size);
void irtx_put(uint16_t data);


#endif
