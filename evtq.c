/*
 * SPDX-License-Identifier: GPL-2.0
 * Copyright (C) 2021 Dahetral Systems
 * Author: David Turvene (dturvene@dahetral.com)
 *
 * event queue
 */

#include <utils.h>
#include <evtq.h>
#include <workers.h>

/**
 * evtq_create - create a queue instance
 *
 * This will malloc an instance of a queue and
 * initialize it.  Notice the NL_INIT_LIST_HEAD macro.
 */
evtq_t* evtq_create(void)
{
	evtq_t *q_p = malloc(sizeof(evtq_t));

	pthread_mutex_init(&q_p->mutex, NULL);
	pthread_cond_init(&q_p->cond, NULL);	
	q_p->len = 0;
	NL_INIT_LIST_HEAD(&q_p->head.list);

	return(q_p);
}

/**
 * evtq_destroy - remove all queue structurs
 *
 * this will destroy mutex, condition and free queue memory
 */
void evtq_destroy(evtq_t* q_p)
{
	if (NULL == q_p)
		return;

	pthread_mutex_destroy(&q_p->mutex);
	pthread_cond_destroy(&q_p->cond);

	free(q_p);
}

/**
 * evtq_enqueue - add an event to the tail of the queue
 * @evtq_p - pointer to event queue
 * @id - the event id to add
 * 
 * lock queue
 * create event, add to queue tail
 * signal condition that there is an new event queued
 * unlock queue
 */
void evtq_enqueue(evtq_t *evtq_p, fsm_events_t evt_id)
{
	struct fsm_event *ep;
	char msg[80];

	pthread_mutex_lock(&evtq_p->mutex);
	
	ep = malloc( sizeof(struct fsm_event) );
	ep->event_id = evt_id;
	nl_list_add_tail(&ep->list, &evtq_p->head.list);
	evtq_p->len++;

	pthread_cond_signal(&evtq_p->cond);
	pthread_mutex_unlock(&evtq_p->mutex);

	dbg_evts(evt_id);
	relax();
}

/**
 * evtq_dequeue - pop an event from head of queue
 * @evtq_p - pointer to event queue
 * @id_p - update this pointer
 *
 * Return: update @id_p with event on queue head
 *
 * lock queue
 * loop while waiting for condition to be set
 *  note: pthread_cond_wait will block waiting on the cond to be set
 * remove event from queue head set the event id
 * free event memory
 * unlock queue
 */ 
void evtq_dequeue(evtq_t *evtq_p, fsm_events_t* id_p)
{
	struct fsm_event *ep;

	/* lock mutex, must be done before cond_wait */
	pthread_mutex_lock(&evtq_p->mutex);

	/* make sure there is something to pop off q */
	while(0 == evtq_p->len) {
		/* this will unlock mutex and then wait on cond */
		/* On return mutex is re-acquired */
		/* if a cond_signal is sent before this is waiting, the signal will be discarded */
		pthread_cond_wait(&evtq_p->cond, &evtq_p->mutex);
	}

	ep = nl_list_first_entry(&evtq_p->head.list, struct fsm_event, list);
	nl_list_del(&ep->list);
	evtq_p->len--;
	*id_p = ep->event_id;
	free(ep);

	pthread_mutex_unlock(&evtq_p->mutex);

	dbg_evts(*id_p);
}

/**
 * evtq_len - 
 * @evtq_p - pointer to event queue
 *
 * Return:
 *   queue len - number of events in queue
 *
 * This is used to check if there are items in queue. 
 * Typically the queue pthread_cond will be set when there are events and evtq_dequeue
 * will wait on it.  This is used when the thread cannot block on the cond_wait.
 */
uint32_t evtq_len(evtq_t *evtq_p)
{
	int len;

	pthread_mutex_lock(&evtq_p->mutex);
	len = evtq_p->len;
	pthread_mutex_unlock(&evtq_p->mutex);
	return(len);
}


