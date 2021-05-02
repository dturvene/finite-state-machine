/*
 * SPDX-License-Identifier: GPL-2.0
 * Copyright (C) 2021 Dahetral Systems
 * Author: David Turvene (dturvene@dahetral.com)
 *
 * Finite State Machine
 */

#include <utils.h>
#include <workers.h>
#include <fsm.h>

fsm_state_t *next_state(fsm_trans_t *fsm_p, fsm_events_t evt_id)
{
	fsm_trans_t *t_p = fsm_p;
	char msg[80];
			
	while (t_p->currst_p != NULL) {
		if (t_p->currst_p == fsm_p->currst_p && t_p->event == evt_id) {
			sprintf(msg, "%s: match %d", fsm_p->currst_p->name, evt_id);
			dbg(msg);
			return (t_p->nextst_p);
		}
		t_p++;
	}

	sprintf(msg, "%s: NO match %d", fsm_p->currst_p->name, evt_id);
	dbg(msg);
	return(NULL);
}

/**
 * fsm
 */
int fsm_run(worker_t* self_p, fsm_events_t evt_id)
{
	fsm_trans_t *fsm_p = self_p->fsm_p;
	fsm_state_t *nextst_p;
	int ret = 0;

	nextst_p = next_state(fsm_p, evt_id);
	if (nextst_p) {

		dbg_trans(self_p->name, fsm_p, nextst_p, evt_id);
			
		/* before transition to next state, run curr state
		 * exit action
		 */
		if (fsm_p->currst_p->exit_action) {
			fsm_p->currst_p->exit_action(fsm_p->currst_p);
		}
		fsm_p->currst_p = nextst_p;

		/* run entry action after state transition */
		if (fsm_p->currst_p->entry_action) {
			fsm_p->currst_p->entry_action(fsm_p->currst_p);
		}			
	} else {
		char msg[120];
		snprintf(msg, sizeof(msg), "No next for curr=%s, evt=%s",
			 fsm_p->currst_p->name, evt_name[evt_id]);
		dbg(msg);
		ret = -1;
	}
	return (ret);
}
