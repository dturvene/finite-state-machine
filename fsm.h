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

static inline void dbg_trans(const char* worker_name, fsm_trans_t *fsm_p,
			     fsm_state_t *nextst_p, fsm_events_t evt_id)
{
	struct timespec ts;
	char buf[120];
	int len;

	if (! debug_flag & DBG_TRANS)
		return;
			
	clock_gettime(CLOCK_MONOTONIC, &ts);
	len=snprintf(buf, sizeof(buf), "%s ts=%ld.%09ld %s to %s for evt=%s\n",
		     worker_name,
		     ts.tv_sec, ts.tv_nsec,
		     fsm_p->currst_p->name, nextst_p->name,
		     evt_name[evt_id]);
	
	/* if cannot fit entire string into buffer, force a newline and null at end */
	if (len >= sizeof(buf)) {
		buf[118] = '\n';
		buf[119] = '\0';
	}
	/* write string to STDOUT */
	write(1, buf, strlen(buf));
}

#endif /* _FSM_H */
