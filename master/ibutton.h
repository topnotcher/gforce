#ifndef IBUTTON_H
#define IBUTTON_H

#define IBUTTON_CMD_READ_ROM 0x33

void ibutton_init(void);
void ibutton_detect_cycle(void);
#endif
