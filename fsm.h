/*
 * SPDX-License-Identifier: GPL-2.0
 * Copyright (C) 2021 Dahetral Systems
 * Author: David Turvene (dturvene@dahetral.com)
 *
 * Finite State Machine definitions
 */

#ifndef _FSM_H
#define _FSM_H

#include <utils.h>
#include <evtq.h>

/**
 * typedef action - generic function pointer for entry and exit actions
 * @arg: 
 */
typedef void (*action)(void *arg);

/**
 * typedef fsm_state - definition of one FSM state
 * @name: string name of state for debugging
 * @entry_action: function to run when state is entered
 * @exit_action: function to run when state is exitting
 */
typedef struct fsm_state {
	const char * const name;
	action entry_action;
	action exit_action;
} fsm_state_t;

/**
 * typedef constraint - transition contstraint function
 * @arg:
 * 
 * Return: a boolean true/false
 */
typedef bool (*constraint)(void *arg);

/**
 * typedef fsm_trans - FSM transition
 * @currst_p - pointer to current state
 * @event - one of defined events
 * @guard - boolean to allow, prevent transition
 * @nextst_p - pointer to the next state if transition succeeds
 */ 
typedef struct fsm_trans {
	fsm_state_t *currst_p;
	fsm_events_t event;
	constraint guard;
	fsm_state_t *nextst_p;
} fsm_trans_t;

/*
 * action debug macro
 */
#define ACT_TRACE() do { \
		if (debug_flag & DBG_DEEP) {				\
			fsm_state_t *state_p = (fsm_state_t*) arg;	\
			printf("%s:%s %s\n", worker_get_name(), __func__, state_p->name); \
		}							\
	} while(0);

/**
 * fsm_init - start FSM (when E_INIT is received)
 * @fsm_p - pointer to FSM[0] transition entry
 * 
 * if there is an entry action, run it
 */
static inline void fsm_init(fsm_trans_t *fsm_p)
{
	/* run FSM init state entry action */
	if (fsm_p->currst_p->entry_action)
		fsm_p->currst_p->entry_action(fsm_p->currst_p);
}

extern int fsm_run(fsm_trans_t * fsm_p, fsm_events_t evt_id);

#endif /* _FSM_H */
