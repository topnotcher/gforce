#include <queue.h>
#ifndef EVENTQ_H
#define EVENTQ_H

void tasks_init(void);
void tasks_run(void);
void task_schedule(void (*cb)(void));

#endif
