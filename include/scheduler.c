#include <scheduler.h>
#include <stdlib.h>
#include <string.h>

task_node * task_list;

inline void scheduler_init(void) {

	TCC0.CCA = 255;
	TCC0.CTRLB |= TC0_CCAEN_bm;
	SCHEDULER_CONTROL_REGISTER = SCHEDULER_PRESCALER_BITS;
	/**
	 * If I need to increase timer resolution:
	 * see also: scheduler.h
	 * TCCR2A |= _BV(WGM21);
	 * OCR2A = 127;
	 */
	SCHEDULER_REGISTER = 0;
	task_list = NULL;
}

void scheduler_register(void (*task_cb)(void), task_freq_t task_freq, task_lifetime_t task_lifetime) {
	cli();

	scheduler_task * task;
	task_node * node;

	task = malloc( sizeof *task );
	memset(task, 0, sizeof *task);
	task->task = task_cb;
	task->freq = task_freq;
	task->lifetime = task_lifetime;
	task->ticks = task_freq;

	node = malloc( sizeof *node );
	memset(node, 0, sizeof *node);
	node->task = task;
	node->next = NULL;

	if ( task_list == NULL ) {
		task_list = node;
		SCHEDULER_INTERRUPT_REGISTER |= SCHEDULER_INTERRUPT_ENABLE_BITS;
		goto end;
	}

	//for misguided reasons, this inserted everything in order of ticks...
	//now it's just inserting things in order of frequency... for no reason.

	//handle the case of replacing the head 
	if ( task_list->task->freq >= task->freq )  {
		node->next = task_list;
		task_list = node;
	} else {

		task_node * tmp = task_list;

		while ( tmp->next != NULL && tmp->next->task->freq < task->freq )
			tmp = tmp->next;

		node->next = tmp->next;
		tmp->next = node;
	}

	end:
		sei();
}

void scheduler_unregister(void (*task_cb)(void)) {
cli();

	if ( task_list == NULL )
		goto end;

	//node we need to remove.
	task_node * node = task_list;
	
	if ( node->task->task == task_cb ) {
		task_list = node->next;
		free(node->task);
		free(node);

		//killed the head: no need to tick!
		if ( task_list == NULL ) 
			SCHEDULER_INTERRUPT_REGISTER &= ~SCHEDULER_INTERRUPT_ENABLE_BITS;

	} else {
		while ( node->next != NULL ) {
			if ( node->next->task->task == task_cb ) {
				//remove the node from the list.
				task_node * tmp = node->next;
				node->next = tmp->next;
				
				//destroy the node.
				free(tmp->task);
				free(tmp);

				break;
			} else {
				node = node->next;
			}
		}
	}
	end:
		sei();
}

SCHEDULER_RUN {
/**
 * @TODO: ISR potentially calls a function => all registers get pushed onto the stack
 * This is true even if no tasks are ready to be called. Might be a worthless optimization, but doing this
 * in assembly could save ~40-60? cycles per invocation.
 */
	//interrupts already disabled.

	SCHEDULER_REGISTER = 0;

	task_node * node = task_list;
	task_node * cur = task_list;
	
	while ( cur != NULL ) {
		node = cur;

		// advance pointer early because
		// if schedule_unregister is called, it could 
		// end up being a dangling pointer (causes AVR restart?)
		cur = node->next;

		if ( --node->task->ticks == 0 ) {
			node->task->task();

			if ( node->task->lifetime != SCHEDULER_RUN_UNLIMITED && --node->task->lifetime == 0 )
				scheduler_unregister(node->task->task);

			// optimal to avoid updating ticks when unregistering.
			else
				node->task->ticks = node->task->freq;

		}
	}
}
