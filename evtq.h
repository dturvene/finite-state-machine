/*
 * SPDX-License-Identifier: GPL-2.0
 * Copyright (C) 2005-2021 Dahetral Systems
 * Author: David Turvene (dturvene@dahetral.com)
 *
 * emacs include file template
 */

#ifndef _EVTQ_H
#define _EVTQ_H

#include <stdbool.h>     /* bool type and true, false values */
#include <inttypes.h>    /* include stdint.h, PRI macros, integer conversions */
#include <unistd.h>      /* write */
#include <stdlib.h>
#include <ctype.h>       /* isalnum */
#include <stdio.h>
#include <string.h>
#include <pthread.h>     /* posix threads */
#include <libnl3/netlink/list.h> /* kernel-ish linked list */

/*
 * fsm_events_t - enum containg all events
 */
typedef enum evt_id {
	EVT_BAD = 0,
	EVT_TIMER,
	EVT_IDLE,
	/* EVT_DONE will cause consumer to exit when it reads
	 * the event even if there are events after it in the 
	 * input queue.
	 */
	EVT_DONE,
	EVT_LAST,
} fsm_events_t;

/*
 * evt_name - mapping from evt_id to a text string for debugging
 */
static const char * const evt_name[] = {
	[EVT_BAD] = "Bad Evt",
	[EVT_TIMER] = "Time Tick",
	[EVT_IDLE] = "Idle, so... peaceful",
	[EVT_DONE] = "DONE, all tasks will exit",
	[EVT_LAST] = "NULL",
};

/**
 * struct fsm_event
 * @list: kernel-style linked list node
 * @event_id: one of the valid events
 */
struct fsm_event {
	struct nl_list_head list;
	fsm_events_t event_id;
};

/**
 * evtq_t - the
 * @len: number of items on queue
 * @head: head of queue
 * @mutex: mutex guarding access to the queue
 * @cond: condition set when an event is added to queue
 *
 * This is user-space implementation of the kernel list management function 
 * https://www.kesrnel.org/doc/html/v5.1/core-api/kernel-api.html#list-management-functions
 * It uses the netlink/list.h macros.
 */
typedef struct {
	int len;
	struct fsm_event head;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
} evtq_t;

static inline void event_show(fsm_events_t evt_id, const char *msg)
{
	char buff[80];
	snprintf(buff, sizeof(buff), "%s %s", msg, evt_name[evt_id]);
	// dbg(buff);
}

extern evtq_t* evtq_create(void);
extern void evtq_destroy(evtq_t* pq);
extern void evtq_push(evtq_t *evtq_p, fsm_events_t id);
extern void evtq_push_all(evtq_t *evtq_pp[], fsm_events_t id);
extern void evtq_pop(evtq_t *evtq_p, fsm_events_t* id_p);
extern uint32_t evtq_len(evtq_t *evtq_p);
extern int evt_ondemand(const char c, evtq_t **evtq_pp);
extern void evt_script(evtq_t **evtq_pp);

#endif /* _EVTQ_H */


