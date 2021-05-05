/*
 * SPDX-License-Identifier: GPL-2.0
 * Copyright (C) 2021 Dahetral Systems
 * Author: David Turvene (dturvene@dahetral.com)
 *
 * timer service API available to other threads
 */

#ifndef _TIMER_H
#define _TIMER_H

#include <stdbool.h>     /* bool type and true, false values */
#include <inttypes.h>    /* include stdint.h, PRI macros, integer conversions */
#include <unistd.h>      /* write */
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>       /* perror, see /usr/include/asm-generic for E-codes */
#include <sys/timerfd.h>
#include <pthread.h>     /* posix threads */
#include <libnl3/netlink/list.h> /* kernel-ish linked list */
#include <evtq.h>
#include <fsm.h>
#include <workers.h>

typedef struct fsmtimer {
	struct nl_list_head list;
	uint32_t timerid;
	fsm_events_t evtid;
	uint64_t tick_ms;
	uint64_t old_tick_ms;
	int fd;
} fsmtimer_t;

typedef struct timer_list {
	fsmtimer_t head;
	pthread_mutex_t mutex;
} timer_list_t;

extern int create_timer(uint32_t timerid, fsm_events_t evtid);
extern int set_timer(uint32_t timerid, uint64_t tick_ms);
extern int stop_timer(uint32_t timerid);
extern uint64_t get_timer(uint32_t timerid);
extern int toggle_timer(uint32_t timerid);
extern void* timer_service_fn(void *arg);
extern fsmtimer_t *find_timer_by_id(uint32_t timerid);
extern fsmtimer_t *find_timer_by_pollfd(int pollfd);
extern void show_timers(void);

static inline uint64_t get_msec(uint32_t timerid)
{
	fsmtimer_t *timer_p = find_timer_by_id(timerid);
	if (!timer_p)
		die("unknown timer");
	return(timer_p->tick_ms);
}

#endif /* _TIMER_H */


