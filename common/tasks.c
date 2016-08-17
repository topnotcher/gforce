#include <stdlib.h>


#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "tasks.h"

static QueueHandle_t event_queue;
static void tasks_thread(void *params);

void tasks_init(void) {
	event_queue = xQueueCreate(10, sizeof(void (*)(void)));

	xTaskCreate(tasks_thread, "main", 128, NULL, tskIDLE_PRIORITY + 2, (TaskHandle_t*)NULL);
}

static void tasks_thread(void *params) {
	while (1) {
		void (*cb)(void) = NULL;

		BaseType_t ret = xQueueReceive(event_queue, &cb, portMAX_DELAY);
		if (ret != pdTRUE || cb == NULL)
			return;

		cb();
	}
}

void task_schedule(void (*cb)(void)) {
	xQueueSend(event_queue, &cb, 0);
}

void task_schedule_isr(void (*cb)(void)) {
	xQueueSendFromISR(event_queue, &cb, NULL);
}
