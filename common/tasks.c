#include <tasks.h>
#include <queue.h>

#include "threads.h"

static queue_t *event_queue;

static uint8_t main_thread_id = 0;

void tasks_init(void) {
	event_queue = queue_create(10);
}

void tasks_run(void) {
	void (*cb)(void) = queue_poll(event_queue);

	if (cb == (void *)0) {
		thread_suspend(&main_thread_id);
		return;
	}

	cb();
}

void task_schedule(void (*cb)(void)) {
	queue_offer(event_queue, cb);
	if (main_thread_id)
		thread_wake(main_thread_id);
}
