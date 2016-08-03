#include <stdlib.h>


#include "FreeRTOS.h"
#include "queue.h"

#include "tasks.h"

static QueueHandle_t event_queue;

void tasks_init(void) {
	event_queue = xQueueCreate(10, sizeof(void (*)(void)));
}

void tasks_run(void) {
	void (*cb)(void) = NULL;

	BaseType_t ret = xQueueReceive(event_queue, &cb, portMAX_DELAY);
	if (ret != pdTRUE || cb == NULL)
		return;

	cb();
}

void task_schedule(void (*cb)(void)) {
	xQueueSend(event_queue, &cb, 0);
}

void task_schedule_isr(void (*cb)(void)) {
	xQueueSendFromISR(event_queue, &cb, NULL);
}
