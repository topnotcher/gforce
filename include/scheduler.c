#include <util/atomic.h>

#include <scheduler.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TASKS 8

static task_node _task_heap[MAX_TASKS];
static task_node * task_list = NULL;

#define _TASK_NODE_EMPTY(node)((node)->task.task == NULL)

//number of "ticks" skipped.
static task_freq_t ticks;

static inline void set_ticks(void);
static void scheduler_remove_node(task_node * rm_node);

inline void scheduler_init(void) {

	//@TODO
	RTC.CTRL = RTC_PRESCALER_DIV1_gc;
	CLK.RTCCTRL = CLK_RTCSRC_RCOSC_gc /*CLK_RTCSRC_ULP_gc*/ | CLK_RTCEN_bm;
	RTC.COMP = 1;
	ticks = 1;
	RTC.CNT = 0;

	//unoccupied blocks have NULL function pointer.
	for (uint8_t i = 0; i < MAX_TASKS; ++i )
		_task_heap[i].task.task = NULL;

}

void scheduler_register(void (*task_cb)(void), task_freq_t task_freq, task_lifetime_t task_lifetime) {
ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {

	scheduler_task * task = NULL;
	task_node * node = NULL;

	//find the first free node
	for (uint8_t i = 0; i < MAX_TASKS; ++i ) {
		if (_TASK_NODE_EMPTY(&_task_heap[i])) {
			node = &_task_heap[i];
			task = &(node->task);
			break;
		}
	}

	//assuming that the loop always finds an available node.

	task->task = task_cb;
	task->freq = task_freq;
	task->lifetime = task_lifetime;
	task->ticks = task_freq;

	node->next = NULL;

	if (task_list == NULL) {
		task_list = node;
		set_ticks();
		SCHEDULER_INTERRUPT_REGISTER |= SCHEDULER_INTERRUPT_ENABLE_BITS;
		return;
	}

	/** Everything gets inserted into the list in order of ticks This way the
	 * ISR can be optimized to tick only when needed.  that said: when
	 * inserting via ticks, an item in the queue may have K < item.freq ticks
	 * remaining until the next run... this could place it ahead of the
	 * inserted item even though it runs less often (the queue needs to be
	 * reordered when something runs???)
	 */
	task_node * cur = task_list;
	while (cur != NULL) {
		cur->task.ticks -= RTC.CNT;
		cur = cur->next;
	}

	//handle the case of replacing the head
	if (task_list->task.ticks >= task->ticks) {
		node->next = task_list;
		task_list = node;
	} else {
		task_node * tmp = task_list;

		while (tmp->next != NULL && tmp->next->task.ticks < task->ticks)
			tmp = tmp->next;

		node->next = tmp->next;
		tmp->next = node;
	}

	set_ticks();
}}

static inline void set_ticks(void) {
	RTC.CNT = 0;
	RTC.COMP = task_list->task.ticks;
	ticks = task_list->task.ticks;
}

void scheduler_unregister(void (*task_cb)(void)) {

	task_node * node = task_list;

	while (node != NULL) {
		if (node->task.task == task_cb)
			scheduler_remove_node(node);
		node = node->next;
	}
}

static void scheduler_remove_node(task_node * rm_node) { ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {

	if (task_list == NULL) return;

	//node we need to remove.
	task_node * node = task_list;
	
	//mark the node unused.
	rm_node->task.task = NULL;

	if (rm_node == node) {
	
		task_list = node->next;

		//killed the head: no need to tick!
		if (task_list == NULL)
			SCHEDULER_INTERRUPT_REGISTER &= ~SCHEDULER_INTERRUPT_ENABLE_BITS;

	} else {
		while (node->next != NULL) {
			if (node->next == rm_node) {
				node->next = rm_node->next;

				break;
			} else {
				node = node->next;
			}
		}
	}

}}

SCHEDULER_RUN {

	task_node * node = task_list;
	task_node * cur = task_list;
	uint8_t reorder = 0;

	while (cur != NULL) {
		node = cur;

		// advance pointer early because scheduler_register might
		// deallocate the node, leaving a dangling pointer.
		cur = node->next;

		if (node->task.ticks <= ticks) {
			node->task.task();

			if (node->task.lifetime != SCHEDULER_RUN_UNLIMITED && --node->task.lifetime == 0) {
				scheduler_remove_node(node);
			} else {
				reorder++;
				node->task.ticks = node->task.freq;
			}
		} else {
			node->task.ticks -= ticks;
		}
	}

	if (task_list != NULL) {
		// the list is in order as of when items are inserted so they can only
		// become out of order when something run and is NOT unregistered.  In
		// this case, the item that was run would have been on top (could have
		// been top N items?)
		if (reorder) {
			task_ticks_t min = task_list->task.ticks;
			cur = task_list->next;
			task_node * prev = task_list;
		
			while (cur != NULL && reorder--) {
				if (cur->task.ticks < min) {
					min = cur->task.ticks;

					task_node * tmp = cur;
					prev->next = cur->next;
					cur = prev->next;
					tmp->next = task_list;
					task_list = tmp;
	
				} else {
					prev = cur;
					cur = cur->next;
				}
			}
		}

		set_ticks();
	}
}
