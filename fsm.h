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
	fsm_state_t *curr;
	int event;
	fsm_state_t *next;
} fsm_trans_t;

static inline fsm_state_t *next_state(fsm_state_t *cs, int event, fsm_trans_t *tbl)
{
	fsm_trans_t *t_p = tbl;

	while (t_p->curr != NULL) {
		if (t_p->curr == cs && t_p->event == event) {
			char msg[80];
			sprintf(msg, "match to %s\n", t_p->next->name);
			dbg(msg);
			return (t_p->next);
		}
		t_p++;
	}
	dbg("No match");
	return(NULL);
}

#endif /* _FSM_H */


