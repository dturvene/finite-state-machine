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
#include "utils.h"
#include "evtq.h"
#include "fsm.h"

typedef struct worker {
	struct nl_list_head list;
	char name[32];
	pthread_t worker_id;
	fsm_trans_t *fsm_p;
	evtq_t *evtq_p;
} worker_t;

typedef struct workers {
	worker_t head;
} workers_t;

/*
 * defined as a global in main program
 */
extern workers_t workers;

inline static worker_t * worker_create(void *(*startfn_p)(void*), char* name)
{
	worker_t *w_p = malloc(sizeof(worker_t));

	strncpy(w_p->name, name, sizeof(w_p->name));
	w_p->fsm_p = NULL;
	w_p->evtq_p = evtq_create();
	if (0 != pthread_create(&w_p->worker_id, NULL, startfn_p, (void *)w_p))
		die("worker_create");
	return (w_p);
}

inline static worker_t *worker_fsm_create(void *(*startfn_p)(void*), char* name, fsm_trans_t* fsm_p)
{
	worker_t *w_p = malloc(sizeof(worker_t));	

	strncpy(w_p->name, name, sizeof(w_p->name));
	w_p->fsm_p = fsm_p; /* must set this before starting thread fsm_init */
	w_p->evtq_p = evtq_create();
	if (0 != pthread_create(&w_p->worker_id, NULL, startfn_p, (void *)w_p))
		die("worker_create");
	return(w_p);
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

inline static const char* worker_get_name(void)
{
	worker_t *w_p = worker_self();
	if (w_p)
		return w_p->name;

	return (NULL);
}

inline static worker_t *worker_find_by_name(const char *name)
{
	worker_t *w_p;	
	nl_list_for_each_entry(w_p, &workers.head.list, list) {
		if (0 == strncmp(w_p->name, name, sizeof(w_p->name)))
			return(w_p);
	}
	return(NULL);
}

inline static void workers_evt_broadcast(fsm_events_t evt_id)
{
	worker_t *w_p;
	nl_list_for_each_entry(w_p, &workers.head.list, list) {
		evtq_enqueue(w_p->evtq_p, evt_id);
	}
}

inline static void workers_evtq_destroy(void)
{
	worker_t *w_p;
	nl_list_for_each_entry(w_p, &workers.head.list, list) {
		evtq_destroy(w_p->evtq_p);
	}
}	

inline static void join_workers(void)
{
	worker_t *w_p;
	nl_list_for_each_entry(w_p, &workers.head.list, list) {
		pthread_join(w_p->worker_id, NULL);
		if (debug_flag & DBG_WORKER)
			printf("%s: joined\n", w_p->name);
	}
}

inline static void show_workers(void)
{
	worker_t *w_p;

	printf("workers\n%-15s:%-12s %-14s\n", "id", "name", "[curr_state]");
	nl_list_for_each_entry(w_p, &workers.head.list, list) {
		printf("%ld:%-12s ", w_p->worker_id, w_p->name);
		w_p->fsm_p ? printf("%s\n", w_p->fsm_p->currst_p->name) : printf("\n");
	}
}

#endif /* _WORKERS_H */
