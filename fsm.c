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
 * dbg_trans - write to stdout detailed information about the FSM state transition
 * @fsm_p - pointer to FSM context
 * @nextst_p - pointer to presumptive next state (before guard check)
 * @evt_id - event id
 *
 * string containing thread, timestamp, evtid, currstate to nextstate
 * This is called before transition guard check.
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

/**
 * next_state - find next state in FSM table and return it
 * @fsm_p - pointer to FSM context
 * @evt_id - event id
 *
 * loop through the FSM transition table, matching curr state and evt_id
 * then return a pointer to the next state.
 *
 * Return: pointer to next state or NULL if no match
 *
 * TODO: make evt_id match in the transition table more efficient.  We already
 * know current state so just a matter of matching the evt_id.
 */
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
 * fsm_run - crank the FSM once for input event
 * @fsm_p - the FSM context
 * @evt_id - the event id
 *
 * - find the next state
 * - if valid (not NULL), check for a guard
 * - if guard, call it and return if fails (false)
 * - otherwise 
 * -  call exit action of the current state
 * -  move to next state
 * -  call entry action of new current state
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
			dbg_verbose("Guard FAILED");
			/* set to guard failed */
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

			dbg_verbose("Guard PASSED");
			/* set to success! */
			ret = 0;
		}
	}
	return (ret);
}

