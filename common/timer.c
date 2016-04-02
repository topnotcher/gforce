#include <util/atomic.h>
#include <stdlib.h>
#include <stdbool.h>
#include "timer.h"
#include "mempool.h"

#define ATTR_ALWAYS_INLINE __attribute__ ((always_inline))

#ifndef MAX_TIMERS
#define MAX_TIMERS 8
#endif

typedef struct {
	void (*task)(void);
	timer_ticks_t freq;
	timer_ticks_t ticks;
	//TIMER_RUN_UNLIMITED = infinite.
	timer_lifetime_t lifetime;
} timer_task;

struct timer_node_st;
typedef struct timer_node_st {
	timer_task task;
	struct timer_node_st *next;
	struct timer_node_st *prev;
} timer_node;

static mempool_t *task_pool;
static timer_node *task_list;
static timer_ticks_t ticks;

static inline void set_ticks(void) ATTR_ALWAYS_INLINE;
static void __del_timer_node(timer_node *rm_node);
static void __del_timer(void (*task_cb)(void));
static void __add_timer_node(timer_node *node, uint8_t adjust);
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

	task_pool = init_mempool(sizeof(timer_node), MAX_TIMERS);
}

/**
 * Allocate memory for a timer and initialize the structure members.
 *
 * @see add_timer()
 */
static timer_node *init_timer(void (*task_cb)(void), timer_ticks_t task_freq,
		timer_lifetime_t task_lifetime) {

	timer_node *node = mempool_alloc(task_pool);

	if (node == NULL)
		return NULL;

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
 * Create and register a timer. Result is _undefined_ if called while executing
 * a timer.
 *
 * @param task_cb callback function to run when timer triggers.
 * @param task_freq frequency to trigger the timer.
 * @param task_lifetime number of times to trigger the timer (or TIMER_RUN_UNLIMITED).
 */
void add_timer(void (*task_cb)(void), timer_ticks_t task_freq, timer_lifetime_t task_lifetime) {
	timer_node * node = init_timer(task_cb, task_freq, task_lifetime);

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		__add_timer_node(node, 1);
		set_ticks();
	}
}

/**
 * Register a timer. This function MUST be called with interrupts disabled. If
 * it is called outside of the timer interrupt, adjust should be set to 1 to
 * indicate that the partial tick should be deducted from the timers. List
 * items are inserted in increasing order of # of ticks. It is assumed that the
 * added node has NULL ->next and ->prev.
 *
 * @param node the node to add to the list
 * @param adjust whether the current timers need to be adjusted.
 *
 * @see add_timer()
 */
static void __add_timer_node(timer_node *node, uint8_t adjust) {
	if (task_list == NULL) {
		task_list = node;
		return;
	}

	//if add_timer is called between ticks.
	if (adjust) {
		timer_node *cur = task_list;
		while (cur != NULL) {
			//this is probably unlikely to be false (though possible)
			if (cur->task.ticks >= RTC.CNT)
				cur->task.ticks -= RTC.CNT;
			else
				cur->task.ticks = 0;
			cur = cur->next;
		}
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

		if (node->next != NULL)
			node->next->prev = node;
	}
}

/**
 * Set the number of ticks until the next interrupt.
 *
 * @TODO the RTC COMP/CNT size must be 16 bits?
 * this could get nasty otherwise.
 */
static inline void set_ticks(void) {
	if (task_list == NULL) {
		TIMER_INTERRUPT_REGISTER &= ~TIMER_INTERRUPT_ENABLE_BITS;
	} else {
		//see xmegaA, p190. Results are insane if SYNCBUSY is not checked.
		//(i.e. the values do not update)
		while (RTC.STATUS&RTC_SYNCBUSY_bm);
		RTC.CNT = 0;
		RTC.COMP = task_list->task.ticks;
		ticks = task_list->task.ticks;
		TIMER_INTERRUPT_REGISTER |= TIMER_INTERRUPT_ENABLE_BITS;
	}
}

/**
 * Must be called with interrupts disabled.
 */
static void __del_timer(void (*task_cb)(void)) {

	timer_node *node = task_list;

	while (node != NULL) {
		timer_node *next = node->next;
		if (node->task.task == task_cb) {
			__del_timer_node(node);
			break;
		}
		node = next;;
	}
}

/**
 * Delete a timer. Result is undefined if called while executing a timer.
 *
 * @param task_cb the callback function registered in the timer.
 */
void del_timer(void (*task_cb)(void)) {
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		__del_timer(task_cb);
		set_ticks();
	}
}

/**
 * Remove a timer node from the list.
 * Must be called with interrupts disabled.
 */
static void __del_timer_node(timer_node *rm_node) {
	if (rm_node == task_list) {
		task_list = rm_node->next;

	} else {
		//this should always be true.
		if (rm_node->prev != NULL)
			rm_node->prev->next = rm_node->next;

		if (rm_node->next != NULL)
			rm_node->next->prev = rm_node->prev;
	}

	mempool_putref(rm_node);
}

/**
 * Note that the list is rebuilt. This is to handle reordering of the list
 * items when node->task.ticks is reset to .freq. Due to this, a task cannot
 * modify timers while it is running; this must be deferred until outside of
 * timer interrupt context.
 */
TIMER_RUN {
	timer_node *node;
	timer_node *cur = task_list;
	task_list = NULL;

	while (cur != NULL) {
		node = cur;
		cur = node->next;

		node->next = NULL;
		node->prev = NULL;

		if (node->task.ticks <= ticks) {
			node->task.task();

			if (node->task.lifetime != TIMER_RUN_UNLIMITED && --node->task.lifetime == 0) {
				mempool_putref(node);
			} else {
				node->task.ticks = node->task.freq;
				__add_timer_node(node,0);
			}
		} else {
			node->task.ticks -= ticks;

			//@TODO this could possibly be done once with the entire tail
			//inserted: since the remaining nodes are not run, they are already
			//ordered correctly. This would require seeking to the tail of the
			//list. The benefit is probably not worth it.
			__add_timer_node(node,0);
		}
	}

	set_ticks();
}
