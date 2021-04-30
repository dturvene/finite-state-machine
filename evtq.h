/*
 * SPDX-License-Identifier: GPL-2.0
 * Copyright (C) 2021 Dahetral Systems
 * Author: David Turvene (dturvene@dahetral.com)
 *
 * events and event queue definitions
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
	EVT_INIT,
	EVT_RED,
	EVT_GREEN,
	EVT_YELLOW,
	EVT_DONE,
	EVT_LAST,
} fsm_events_t;

/*
 * evt_name - mapping from evt_id to a text string for debugging
 */
static const char * const evt_name[] = {
	[EVT_BAD] = "Bad Evt",
	[EVT_TIMER] = "Time Tick",
	[EVT_IDLE] = "Idle",
	[EVT_INIT] = "INIT",
	[EVT_RED] = "RED",
	[EVT_GREEN] = "GREEN",
	[EVT_YELLOW] = "YELLOW",
	[EVT_DONE] = "DONE",
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

static inline void evt_show(fsm_events_t evt_id, const char *msg)
{
	char buff[80];
	snprintf(buff, sizeof(buff), "%s %s", msg, evt_name[evt_id]);
	dbg(buff);
}

extern evtq_t* evtq_create(void);
extern void evtq_destroy(evtq_t* q_p);
extern void evtq_destroy_all(evtq_t** q_pp);
extern void evtq_push(evtq_t *evtq_p, fsm_events_t id);
extern int evtq_show(evtq_t *evtq_p);
extern void evtq_pop(evtq_t *evtq_p, fsm_events_t* id_p);
extern uint32_t evtq_len(evtq_t *evtq_p);
extern int evt_ondemand(const char c);
extern void evt_script(void);

#endif /* _EVTQ_H */


