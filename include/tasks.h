#ifndef EVENTQ_H
#define EVENTQ_H

void tasks_init(void);
void task_schedule(void (*cb)(void));
void task_schedule_isr(void (*cb)(void));

#endif
