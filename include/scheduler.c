#include <util/atomic.h>

#include <scheduler.h>
#include <stdlib.h>
#include <string.h>

static task_node * task_list;

//number of "ticks" skipped.
static task_freq_t ticks;

static inline void set_ticks(void);
static void scheduler_remove_node(task_node * rm_node);
static inline void scheduler_free_node(task_node * node);

inline void scheduler_init(void) {

	//@TODO
	RTC.CTRL = RTC_PRESCALER_DIV1_gc;
	CLK.RTCCTRL = CLK_RTCSRC_RCOSC_gc /*CLK_RTCSRC_ULP_gc*/ | CLK_RTCEN_bm;
	RTC.COMP = 1;
	ticks = 1;
	RTC.CNT = 0;

	task_list = NULL;
}

void scheduler_register(void (*task_cb)(void), task_freq_t task_freq, task_lifetime_t task_lifetime) { ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {

	scheduler_task * task;
	task_node * node;

	task = malloc( sizeof *task );
	task->task = task_cb;
	task->freq = task_freq;
	task->lifetime = task_lifetime;
	task->ticks = task_freq;

	node = malloc( sizeof *node );
	node->task = task;
	node->next = NULL;

	if ( task_list == NULL ) {
		task_list = node;
		set_ticks();
		SCHEDULER_INTERRUPT_REGISTER |= SCHEDULER_INTERRUPT_ENABLE_BITS;
		return;
	} 

	/**
	 * Everything gets inserted into the list in order of ticks
	 * This way the ISR can be optimized to tick only when needed.
	 * that said: when inserting via ticks, an item in the queue may have K < item.freq ticks 
	 * remaining until the next run... this could place it ahead of the inserted item even though 
	 * it runs less often (the queue needs to be reordered when something runs???)
	 */
	task_node * cur = task_list;
	while ( cur != NULL ) {
		cur->task->ticks -= RTC.CNT;
		cur = cur->next;
	}

	//handle the case of replacing the head
	if ( task_list->task->ticks >= task->ticks ) {
		node->next = task_list;
		task_list = node;
	} else {

		task_node * tmp = task_list;

		while ( tmp->next != NULL && tmp->next->task->ticks < task->ticks )
			tmp = tmp->next;

		node->next = tmp->next;
		tmp->next = node;
	}

	set_ticks();
}}

static inline void set_ticks(void) {
	RTC.CNT = 0;
	RTC.COMP = task_list->task->ticks;
	ticks = task_list->task->ticks;
}

void scheduler_unregister(void (*task_cb)(void) ) {

	task_node * node = task_list;

	while ( node != NULL ) {
		if ( node->task->task == task_cb )
			scheduler_remove_node(node);
		node = node->next;
	}
}

static void scheduler_remove_node(task_node * rm_node) { ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {

	if ( task_list == NULL ) return;

	//node we need to remove.
	task_node * node = task_list;
	
	if ( rm_node == node ) {
		task_list = node->next;
		free(node->task);
		free(node);

		//killed the head: no need to tick!
		if ( task_list == NULL ) 
			SCHEDULER_INTERRUPT_REGISTER &= ~SCHEDULER_INTERRUPT_ENABLE_BITS;

	} else {
		while ( node->next != NULL ) {
			if ( node->next == rm_node ) {
				node->next = rm_node->next;
				scheduler_free_node(rm_node);

				break;
			} else {
				node = node->next;
			}
		}
	}

}}

static inline void scheduler_free_node(task_node * node) {
	free(node->task);
	free(node);
}

/**
 * @TODO: ISR potentially calls a function => all registers get pushed onto the stack
 * This is true even if no tasks are ready to be called. Might be a worthless optimization, but doing this
 * in assembly could save ~40-60? cycles per invocation.
 */
SCHEDULER_RUN {

	task_node * node = task_list;
	task_node * cur = task_list;
	uint8_t reorder = 0;

	while ( cur != NULL ) {
		node = cur;

		// advance pointer early because
		// if schedule_unregister is called, it could 
		// end up being a dangling pointer (causes AVR restart?)
		cur = node->next;

		if ( (node->task->ticks -= ticks) == 0 ) {

			node->task->task();

			if ( node->task->lifetime != SCHEDULER_RUN_UNLIMITED && --node->task->lifetime == 0 ) {
				scheduler_remove_node(node);

			} else {
				reorder++;
				node->task->ticks = node->task->freq;
			}

		}
	}

	if ( task_list != NULL ) {
		//the list is in order as of when items are inserted 
		//so they can only become out of order when something run and is NOT unregistered. 
		//In this case, the item that was run would have been on top (could have been top N items?)
		if ( reorder ) {
			task_ticks_t min = task_list->task->ticks;
			cur = task_list->next;
			task_node * prev = task_list;
		
			while ( cur != NULL && reorder--) {
				if ( cur->task->ticks < min ) {
					min = cur->task->ticks;

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
