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

/**
 * dbg_trans -
 *
 */
void dbg_trans(fsm_trans_t *fsm_p, fsm_state_t *nextst_p, fsm_events_t evt_id)
{
	struct timespec ts;
	char buf[120];
	int len;

	if (!(debug_flag & DBG_TRANS))
		return;
			
	clock_gettime(CLOCK_MONOTONIC, &ts);
	len=snprintf(buf, sizeof(buf), "%s:ts=%ld.%3ld evt=%s trans %s to %s\n",
		     worker_get_name(),
		     ts.tv_sec%100, ts.tv_nsec/(int)1e6,
		     evt_name[evt_id],
		     fsm_p->currst_p->name, nextst_p?nextst_p->name:"no next");
	
	/* if cannot fit entire string into buffer, force a newline and null at end */
	if (len >= sizeof(buf)) {
		buf[118] = '\n';
		buf[119] = '\0';
	}
	/* write string to STDOUT */
	write(1, buf, strlen(buf));
}

fsm_state_t *next_state(fsm_trans_t *fsm_p, fsm_events_t evt_id)
{
	fsm_trans_t *t_p = fsm_p;
	char msg[80];
			
	while (t_p->currst_p != NULL) {
		if (t_p->currst_p == fsm_p->currst_p && t_p->event == evt_id) {
			sprintf(msg, "%s: match %s", fsm_p->currst_p->name, evt_name[evt_id]);
			dbg_verbose(msg);
			return (t_p->nextst_p);
		}
		t_p++;
	}

	sprintf(msg, "%s: NO match %s", fsm_p->currst_p->name, evt_name[evt_id]);
	dbg_verbose(msg);
	return(NULL);
}

/**
 * fsm_run -
 *
 * Return:
 *  -1: FSM failure, 
 *   0: failed transition to next state (guard failure)
 *   1: success transition to next state
 */
int fsm_run(fsm_trans_t* fsm_p, fsm_events_t evt_id)
{
	fsm_state_t *nextst_p;
	int ret = -1;  /* set to failed */ 

	nextst_p = next_state(fsm_p, evt_id);
	dbg_trans(fsm_p, nextst_p, evt_id);
	
	if (nextst_p) {
		/* check if guard and run it, if guard fails set ret to 1 */
		if (fsm_p->guard && (false == fsm_p->guard(fsm_p)))
		{
			ret = 1;
		} else {
			/* before transition to next state, run curr state
			 * exit action
			 */
			if (fsm_p->currst_p->exit_action) {
				fsm_p->currst_p->exit_action(fsm_p->currst_p);
			}

			/* update currst to nextst */
			fsm_p->currst_p = nextst_p;

			/* run currst entry action after state transition */
			if (fsm_p->currst_p->entry_action) {
				fsm_p->currst_p->entry_action(fsm_p->currst_p);
			}

			/* set to success! */
			ret = 0;
		}
	}
	return (ret);
}

