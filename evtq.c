/*
 * SPDX-License-Identifier: GPL-2.0
 * Copyright (C) 2005-2021 Dahetral Systems
 * Author: David Turvene (dturvene@dahetral.com)
 *
 * event queue
 */

#include <utils.h>
#include <evtq.h>

extern int debug_flag;
extern bool event_loop_done;
extern char scriptfile[];

/**
 * evtq_create - create a queue instance
 *
 * This will malloc an instance of a queue and
 * initialize it.  Notice the NL_INIT_LIST_HEAD macro.
 */
evtq_t* evtq_create(void)
{
	evtq_t *pq = malloc(sizeof(evtq_t));

	pthread_mutex_init(&pq->mutex, NULL);
	pthread_cond_init(&pq->cond, NULL);	
	pq->len = 0;
	NL_INIT_LIST_HEAD(&pq->head.list);

	return(pq);
}

/**
 * evtq_destroy - remove all queue structurs
 *
 * this will destroy mutex, condition and free queue memory
 */
void evtq_destroy(evtq_t* pq)
{
	if (NULL == pq)
		return;

	pthread_mutex_destroy(&pq->mutex);
	pthread_cond_destroy(&pq->cond);

	free(pq);
}

/**
 * evtq_push - add an event to the tail of the queue
 * @evtq_p - pointer to event queue
 * @id - the event id to add
 * 
 * lock queue
 * create event, add to queue tail
 * signal condition that there is an new event queued
 * unlock queue
 */
void evtq_push(evtq_t *evtq_p, fsm_events_t id)
{
	struct fsm_event *ep;

	pthread_mutex_lock(&evtq_p->mutex);
	
	ep = malloc( sizeof(struct fsm_event) );
	ep->event_id = id;
	nl_list_add_tail(&ep->list, &evtq_p->head.list);
	evtq_p->len++;

	pthread_cond_signal(&evtq_p->cond);
	pthread_mutex_unlock(&evtq_p->mutex);

	relax();
}

/**
 * evt_push_all - 
 */
void evtq_push_all(evtq_t *evtq_pp[], fsm_events_t id)
{
	evtq_push(evtq_pp[0], id);
	evtq_push(evtq_pp[1], id);
}

/**
 * evtq_pop - pop an event from head of queue
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
void evtq_pop(evtq_t *evtq_p, fsm_events_t* id_p)
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
}

/**
 * evtq_len - 
 * @evtq_p - pointer to event queue
 *
 * Return:
 *   queue len - number of events in queue
 *
 * This is used to check if there are items in queue. 
 * Typically the queue pthread_cond will be set when there are events and evtq_pop
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

/**
 * evtq_show - dump of events in queue
 * @evtq_p - pointer to event queue
 *
 * This is used for diagnostic debugging, queues empty too fast
 * to use this during runtime.
 */
int evtq_show(evtq_t *evtq_p)
{
	struct fsm_event *ep;
	char msg[80];
	int len;

	pthread_mutex_lock(&evtq_p->mutex);

	if (nl_list_empty(&evtq_p->head.list)) {
		dbg("q empty");
	}

	nl_list_for_each_entry(ep, &evtq_p->head.list, list) {
		sprintf(msg, "%d: id=%u", len++, ep->event_id);
		dbg(msg);
	}
	sprintf(msg, "qsize: %d", evtq_p->len);
	dbg(msg);

	pthread_mutex_unlock(&evtq_p->mutex);
}

/* forward definition */
int evt_ondemand(const char c, evtq_t **evtq_pp);

/**
 * evt_script - load events from a file to added to event queue
 * @evtq_p - pointer to event queue
 *
 * The input file is reference by the global scriptfile var.
 * 
 * This is just a shell to read the file for symbolic event ids.  The 
 * events are translate from symbolic to events enum in evt_ondemand()
 */
void evt_script(evtq_t **evtq_pp)
{
	FILE *fin;
	int len, i;
	char buf[80];
	
	if (NULL == (fin=fopen(scriptfile, "r")))
		die("unknown fname");

	while (NULL != fgets(buf, sizeof(buf), fin)) {

		/* skip comments and empty lines */
		if (buf[0] == '#' || buf[0] == '\n')
			continue;
		
		len = strlen(buf);
		if (debug_flag)
			printf("buf=%s len=%d\n", buf, len);
		
		for (int i=0; i<len; i++)
			if (isalnum(buf[i]))
				evt_ondemand(buf[i], evtq_pp);
	}

	fclose(fin);
}

/**
 * evt_ondemand - translate symbolic event to internal and push to event q
 *
 * @c: the symbolic event id
 * @evtq_pp - array of event queue pointers
 * 
 * Each event can be symbolically represented as a single char.  This 
 * function maps this to an internal events id (from an enum) and
 * pushes it to the given event queue.
 */
int evt_ondemand(const char c, evtq_t **evtq_pp)
{
	
	switch(c) {
	case 'h':
		printf("\tq: quit workers\n");
		printf("\tx: exit producer and workers (gracefully)\n");
		printf("\ti: %s\n", evt_name[EVT_IDLE]);
		printf("\tt: %s\n", evt_name[EVT_TIMER]);
		printf("\tr: run event input script %s\n", scriptfile);
		printf("\ts: sleep 5000ms\n");
		printf("\tdefault: %s\n", evt_name[EVT_IDLE]);
		break;
	case 'q':
		/* just exit event threads */
		evtq_push_all(evtq_pp, EVT_DONE);
		break;
	case 'x':
		/* exit event threads and main */
		evtq_push_all(evtq_pp, EVT_DONE);		
		event_loop_done = true;
		break;
	case 'i':
		evtq_push_all(evtq_pp, EVT_IDLE);
		break;
	case 't':
		evtq_push_all(evtq_pp, EVT_TIMER);
		break;
	case 'r':
		evt_script(evtq_pp);
		break;
	case 's':
		dbg("napping for 5000");		
		nap(5000);
		dbg("after nap");
		break;
	default:
	{
		char msg[80];
		sprintf(msg, "unknown command: '%c'", c);
		dbg(msg);
	}
	break;
	} /* switch */
}