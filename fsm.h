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

typedef void (*action)(void *arg);
typedef struct fsm_state {
	const char * const name;
	action entry_action;
	action exit_action;
} fsm_state_t;

typedef bool (*constraint)(void *arg);
typedef struct fsm_trans {
	fsm_state_t *currst_p;
	int event;
	constraint guard;
	fsm_state_t *nextst_p;
} fsm_trans_t;


static inline void fsm_init(fsm_trans_t *fsm_p)
{
	/* run FSM init state entry action */
	if (fsm_p->currst_p->entry_action)
		fsm_p->currst_p->entry_action(fsm_p->currst_p);
}

extern int fsm_run(fsm_trans_t * fsm_p, fsm_events_t evt_id);

#endif /* _FSM_H */
