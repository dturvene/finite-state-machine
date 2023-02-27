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
	E_BAD = 0,
	E_LIGHT,
	E_BLINK,
	E_INIT,
	E_RED,
	E_GREEN,
	E_YELLOW,
	E_BUTTON,
	E_DONE,
	E_TIMER,
	E_LAST,
} fsm_events_t;

/*
 * evt_name - mapping from evt_id to a text string for debugging
 */
static const char * const evt_name[] = {
	[E_BAD] = "BAD EVT",
	[E_LIGHT] = "LIGHT TIMER",
	[E_BLINK] = "WALK BLINK",
	[E_INIT] = "INIT",
	[E_RED] = "RED",
	[E_GREEN] = "GREEN",
	[E_YELLOW] = "YELLOW",
	[E_BUTTON] = "BUTTON",
	[E_DONE] = "DONE",
	[E_TIMER] = "TIMER TEST",
	[E_LAST] = "LAST",
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

/**
 * _dbg_evts - create a debug string for event 
 * @func - the current function
 * @evt_id - the event id to show
 *
 * print info about current event
 */
static inline void _dbg_evts(const char *func, fsm_events_t evt_id)
{
	char buf[120];
	snprintf(buf, sizeof(buf), "%lu:%s %s\n", pthread_self(), func, evt_name[evt_id]);	
	write(1, buf, strlen(buf));
}

#define dbg_evts(evt_id) if (debug_flag & DBG_EVTS) _dbg_evts(__func__, evt_id);

extern evtq_t* evtq_create(void);
extern void evtq_destroy(evtq_t* q_p);
extern void evtq_destroy_all(evtq_t** q_pp);
extern void evtq_enqueue(evtq_t *evtq_p, fsm_events_t id);
extern void evtq_dequeue(evtq_t *evtq_p, fsm_events_t* id_p);
extern uint32_t evtq_len(evtq_t *evtq_p);
extern int evt_parse_buf(char const *buf);
extern void evt_script(void);
extern void evt_producer(void);

#endif /* _EVTQ_H */


