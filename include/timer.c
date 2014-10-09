#include <util/atomic.h>

#include <timer.h>
#include <stdlib.h>
#include <string.h>
#include <chunkpool.h>

#ifndef MAX_TIMERS
#define MAX_TIMERS 8
#endif

typedef struct {
	void (* task)(void);
	timer_ticks_t freq;
	timer_ticks_t ticks;
	//TIMER_RUN_UNLIMITED = infinite.
	timer_lifetime_t lifetime;
} timer_task;

struct timer_node_st;
typedef struct timer_node_st {
	timer_task task;
	struct timer_node_st * next;
	struct timer_node_st * prev;
} timer_node;

static chunkpool_t * task_pool;
static timer_node * task_list;
static timer_ticks_t ticks;

#define _TIMER_NODE_EMPTY(node)((node)->task.task == NULL)

static inline void set_ticks(void);
static inline void timer_remove_node(timer_node * rm_node);
static inline void __del_timer(void (*task_cb)(void));
static inline void __add_timer_node(timer_node * node);
static timer_node *init_timer(void (*task_cb)(void), timer_ticks_t task_freq,
		timer_lifetime_t task_lifetime);

/**
 * Initialize the timers: run at boot.
 */
void init_timers(void) {

	//@TODO
	RTC.CTRL = RTC_PRESCALER_DIV1_gc;
	CLK.RTCCTRL = CLK_RTCSRC_RCOSC_gc /*CLK_RTCSRC_ULP_gc*/ | CLK_RTCEN_bm;
	RTC.COMP = 1;
	ticks = 1;
	RTC.CNT = 0;

	task_pool = chunkpool_create(sizeof(timer_node), MAX_TIMERS);
}

/**
 * @see add_timer()
 */
static timer_node *init_timer(void (*task_cb)(void), timer_ticks_t task_freq,
		timer_lifetime_t task_lifetime) {

	timer_node *node = (timer_node*)chunkpool_acquire(task_pool);
	timer_task *task = &(node->task);

	task->task = task_cb;
	task->freq = task_freq;
	task->lifetime = task_lifetime;
	task->ticks = task_freq;
	node->prev = NULL;
	node->next = NULL;

	return node;
}

/**
 * Create a timer.
 *
 * @param task_cb callback function to run when timer triggers.
 * @param task_freq frequency to trigger the timer.
 * @param task_lifetime number of times to trigger the timer (or TIMER_RUN_UNLIMITED).
 */
void add_timer(void (*task_cb)(void), timer_ticks_t task_freq, timer_lifetime_t task_lifetime) {
	timer_node * node = init_timer(task_cb, task_freq, task_lifetime);

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		__add_timer_node(node);
	}
}

/**
 * Add a timer node to the list. Must be called with interrupts disabled.
 * @see add_timer()
 */
static inline void __add_timer_node(timer_node * node) {
	if (task_list == NULL) {
		task_list = node;
		set_ticks();
		TIMER_INTERRUPT_REGISTER |= TIMER_INTERRUPT_ENABLE_BITS;
		return;
	}

	timer_node * cur = task_list;
	while (cur != NULL) {
		cur->task.ticks -= RTC.CNT;
		cur = cur->next;
	}

	//handle the case of replacing the head
	if (task_list->task.ticks >= node->task.ticks) {
		node->next = task_list;
		task_list->prev = node;
		task_list = node;
	} else {
		timer_node * tmp = task_list;

		while (tmp->next != NULL && tmp->next->task.ticks < node->task.ticks)
			tmp = tmp->next;

		node->next = tmp->next;
		tmp->next = node;
		node->prev = tmp;
	}

	set_ticks();
}

/**
 * Set the number of ticks until the next interrupt.
 *
 * @TODO the RTC COMP/CNT size must be 16 bits?
 * this could get nasty otherwise.
 */
static inline void set_ticks(void) {
	RTC.CNT = 0;
	RTC.COMP = task_list->task.ticks;
	ticks = task_list->task.ticks;
}

/**
 * Must be called with interrupts disabled.
 */
static inline void __del_timer(void (*task_cb)(void)) {
	timer_node * node = task_list;

	while (node != NULL) {
		if (node->task.task == task_cb)
			timer_remove_node(node);
		node = node->next;
	}
}

/**
 * Delete a timer.
 *
 * @param task_cb the callback function registered in the timer.
 */
void del_timer(void (*task_cb)(void)) {
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		__del_timer(task_cb);
	}
}

/**
 * Remove a timer node from the list.
 * Must be called with interrupts disabled.
 */
static inline void timer_remove_node(timer_node * rm_node) {
	if (task_list == NULL) return;

	if (rm_node == task_list) {
		task_list = rm_node->next;

		if (task_list == NULL)
			TIMER_INTERRUPT_REGISTER &= ~TIMER_INTERRUPT_ENABLE_BITS;
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
static inline void timer_reorder_tasks(uint8_t reorder) {
	timer_ticks_t min = task_list->task.ticks;
	timer_node * cur = task_list->next;

	while (cur != NULL && reorder--) {
		if (cur->task.ticks < min) {
			timer_node * new_head = cur;

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

/**
 * Timer interrupt handling.
 */
TIMER_RUN {
	timer_node * node = task_list;
	timer_node * cur = task_list;
	uint8_t reorder = 0;

	while (cur != NULL) {
		node = cur;
		cur = node->next;

		if (node->task.ticks <= ticks) {
			node->task.task();

			if (node->task.lifetime != TIMER_RUN_UNLIMITED && --node->task.lifetime == 0) {
				timer_remove_node(node);
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
			timer_reorder_tasks(reorder);

		set_ticks();
	}
}
