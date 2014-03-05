#ifndef IBUTTON_H
#define IBUTTON_H

#define IBUTTON_CMD_READ_ROM 0x33

void ibutton_init(void);
int8_t ibutton_detect_cycle(void);

void ibutton_run(void);

void ibutton_switchto(void) __attribute__((naked));
void ibutton_switchfrom(void) __attribute__((naked));
#endif

