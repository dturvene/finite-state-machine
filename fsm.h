/*
 * SPDX-License-Identifier: GPL-2.0
 * Copyright (C) 2021 Dahetral Systems
 * Author: David Turvene (dturvene@dahetral.com)
 *
 * Finite State Machine definitions
 */

#ifndef _FSM_H
#define _FSM_H

typedef void (*action)(void *arg);
typedef struct fsm_state {
	const char * const name;
	action entry_action;
	action exit_action;
} fsm_state_t;

typedef struct fsm_trans {
	fsm_state_t *currst_p;
	int event;
	fsm_state_t *nextst_p;
} fsm_trans_t;

static inline void fsm_init(fsm_trans_t *fsm_p)
{
	fsm_p->currst_p = fsm_p[0].currst_p;
	printf("%s: curr=%s\n", __func__, fsm_p->currst_p->name);
		
	/* run FSM state entry action */
	if (fsm_p->currst_p->entry_action)
		fsm_p->currst_p->entry_action(fsm_p->currst_p);
}

static inline fsm_state_t *next_state(fsm_trans_t *fsm_p, int event)
{
	fsm_trans_t *t_p = fsm_p;

	while (t_p->currst_p != NULL) {
		if (t_p->currst_p == fsm_p->currst_p && t_p->event == event) {
			char msg[80];
			sprintf(msg, "match to %s\n", t_p->nextst_p->name);
			dbg(msg);
			return (t_p->nextst_p);
		}
		t_p++;
	}
	dbg("No match");
	return(NULL);
}

#endif /* _FSM_H */
