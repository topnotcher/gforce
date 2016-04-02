#include <tasks.h>
#include <queue.h>

static queue_t * event_queue;

void tasks_init(void) {
	event_queue = queue_create(10);
}

void tasks_run(void) {
	void (*cb)(void) = queue_poll(event_queue);

	if (cb == (void*)0)
		return;

	cb();
}

void task_schedule(void (*cb)(void)) {
	queue_offer(event_queue, cb);
}
