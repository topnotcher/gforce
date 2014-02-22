#include <queue.h>
#ifndef EVENTQ_H
#define EVENTQ_H

void eventq_init(void);
void eventq_run(void);
void eventq_offer(void (*cb)(void));

#endif
