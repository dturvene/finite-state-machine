/*
 * SPDX-License-Identifier: GPL-2.0
 * Copyright (C) 2021 Dahetral Systems
 * Author: David Turvene (dturvene@dahetral.com)
 *
 * worker thread management
 */

#ifndef _WORKERS_H
#define _WORKERS_H

#include <stdbool.h>     /* bool type and true, false values */
#include <inttypes.h>    /* include stdint.h, PRI macros, integer conversions */
#include <unistd.h>      /* write */
#include <stdlib.h>
#include <ctype.h>       /* isalnum */
#include <stdio.h>
#include <string.h>
#include <pthread.h>     /* posix threads */
#include <libnl3/netlink/list.h> /* kernel-ish linked list */
#include <evtq.h>
#include <fsm.h>

typedef struct worker {
	struct nl_list_head list;
	char name[32];
	pthread_t worker_id;
	fsm_trans_t *fsm;
	evtq_t *evtq_p;
} worker_t;

typedef struct workers {
	worker_t head;
} workers_t;

workers_t workers;

inline static worker_t * worker_create(void *(*startfn_p)(void*), char* name)
{
	worker_t *w_p = malloc(sizeof(worker_t));

	strncpy(w_p->name, name, sizeof(w_p->name));

	w_p->evtq_p = evtq_create();
	if (0 != pthread_create(&w_p->worker_id, NULL, startfn_p, (void *)&workers))
		die("worker_create");
	return (w_p);
}

inline static worker_t *fsm_create(void *(*startfn_p)(void*), char* name, fsm_trans_t* fsm)
{
	worker_t *w_p;

	w_p = worker_create(startfn_p, name);
	w_p->fsm = fsm;
}

inline static void worker_list_create()
{
	NL_INIT_LIST_HEAD(&workers.head.list);
}

inline static void worker_list_add(worker_t *w_p)
{
	nl_list_add_tail(&w_p->list, &workers.head.list);
}

inline static worker_t *worker_first()
{
	worker_t *w_p;
	w_p = nl_list_first_entry(&workers.head.list, worker_t, list);
	return (w_p);
}

inline static worker_t *worker_find_id(pthread_t id)
{
	worker_t *w_p;
	nl_list_for_each_entry(w_p, &workers.head.list, list) {
		if (w_p->worker_id == id)
			return(w_p);
	}
	return(NULL);
}

inline static worker_t *worker_self(void)
{
	return worker_find_id(pthread_self());
}

inline static worker_t *worker_find_name(const char *name)
{
	worker_t *w_p;	
	nl_list_for_each_entry(w_p, &workers.head.list, list) {
		if (0 == strncmp(w_p->name, name, sizeof(w_p->name)))
			return(w_p);
	}
	return(NULL);
}

inline static void workers_evt_push(fsm_events_t evt_id)
{
	worker_t *w_p;
	char msg[80];

	nl_list_for_each_entry(w_p, &workers.head.list, list) {
		evtq_push(w_p->evtq_p, evt_id);
	}
}	

inline static void join_workers()
{
	worker_t *w_p;
	nl_list_for_each_entry(w_p, &workers.head.list, list) {
		pthread_join(w_p->worker_id, NULL);
	}
}

inline static void show_workers()
{
	worker_t *w_p;

	printf("workers and incoming evtq:\n");
	nl_list_for_each_entry(w_p, &workers.head.list, list) {
		printf("worker=%ld name=%s\n", w_p->worker_id, w_p->name);
		evtq_show(w_p->evtq_p);
	}
}

#endif /* _WORKERS_H */
