#include <util/atomic.h>

#include <scheduler.h>
#include <stdlib.h>
#include <string.h>
#include <chunkpool.h>

#ifndef MAX_TASKS
#define MAX_TASKS 8
#endif

typedef struct {
	void (* task)(void);
	task_freq_t freq;
	task_ticks_t ticks;
	//SCHEDULER_RUN_UNLIMITED = infinite.
	task_lifetime_t lifetime;
} scheduler_task;

struct task_node_st;
typedef struct task_node_st {
	scheduler_task task;
	struct task_node_st * next;
	struct task_node_st * prev;
} task_node;

static chunkpool_t * task_pool;
static task_node * task_list = NULL;
static task_freq_t ticks;

#define _TASK_NODE_EMPTY(node)((node)->task.task == NULL)

static inline void set_ticks(void);
static void scheduler_remove_node(task_node * rm_node);

inline void scheduler_init(void) {

	//@TODO
	RTC.CTRL = RTC_PRESCALER_DIV1_gc;
	CLK.RTCCTRL = CLK_RTCSRC_RCOSC_gc /*CLK_RTCSRC_ULP_gc*/ | CLK_RTCEN_bm;
	RTC.COMP = 1;
	ticks = 1;
	RTC.CNT = 0;

	task_pool = chunkpool_create(sizeof(task_node), MAX_TASKS);
}

void scheduler_register(void (*task_cb)(void), task_freq_t task_freq, task_lifetime_t task_lifetime) {
	scheduler_task * task = NULL;
	task_node * node = NULL;

	//@TODO check return
	node = (task_node*)chunkpool_acquire(task_pool);
	task = &(node->task);

	task->task = task_cb;
	task->freq = task_freq;
	task->lifetime = task_lifetime;
	task->ticks = task_freq;

	node->prev = NULL;
	node->next = NULL;

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		if (task_list == NULL) {
			task_list = node;
			set_ticks();
			SCHEDULER_INTERRUPT_REGISTER |= SCHEDULER_INTERRUPT_ENABLE_BITS;
			return;
		}

		task_node * cur = task_list;
		while (cur != NULL) {
			cur->task.ticks -= RTC.CNT;
			cur = cur->next;
		}

		//handle the case of replacing the head
		if (task_list->task.ticks >= task->ticks) {
			node->next = task_list;
			task_list->prev = node;
			task_list = node;
		} else {
			task_node * tmp = task_list;

			while (tmp->next != NULL && tmp->next->task.ticks < task->ticks)
				tmp = tmp->next;
			
			node->next = tmp->next;
			tmp->next = node;
			node->prev = tmp;
		}

		set_ticks();
	}
}

static inline void set_ticks(void) {
	RTC.CNT = 0;
	RTC.COMP = task_list->task.ticks;
	ticks = task_list->task.ticks;
}

/**
 * Entire function wrapped in ATOMIC_BLOCK: must avoid any interrupts modifying
 * the task list while iterating.
 */
void scheduler_unregister(void (*task_cb)(void)) { ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {

	task_node * node = task_list;

	while (node != NULL) {
		if (node->task.task == task_cb)
			scheduler_remove_node(node);
		node = node->next;
	}
}}

/**
 * Only call from an atomic block.
 */
static void scheduler_remove_node(task_node * rm_node) {
	if (task_list == NULL) return;

	if (rm_node == task_list) {
		task_list = rm_node->next;

		if (task_list == NULL)
			SCHEDULER_INTERRUPT_REGISTER &= ~SCHEDULER_INTERRUPT_ENABLE_BITS;
	} else {
		//this should always be true.
		if (rm_node->prev != NULL)
			rm_node->prev->next = rm_node->next;

		if (rm_node->next != NULL)
			rm_node->next->prev = rm_node->prev;
	}

	chunkpool_putref(rm_node);
}

/**
 * only call with interrupts disabled.
 */
static inline void scheduler_reorder_tasks(uint8_t reorder) {
	task_ticks_t min = task_list->task.ticks;
	task_node * cur = task_list->next;

	while (cur != NULL && reorder--) {
		if (cur->task.ticks < min) {
			task_node * new_head = cur;

			min = cur->task.ticks;
			
			//should always be true.
			if (cur->prev != NULL)
				cur->prev->next = cur->next;

			if (cur->next != NULL)
				cur->next->prev = cur->next;

			cur = cur->next;

			new_head->next = task_list;
			task_list->prev = new_head;
			task_list = new_head;

		} else {
			cur = cur->next;
		}
	}
}

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
		if (reorder)
			scheduler_reorder_tasks(reorder);

		set_ticks();
	}
}
