/*
 * SPDX-License-Identifier: GPL-2.0
 * Copyright (C) 2021 Dahetral Systems
 * Author: David Turvene (dturvene@dahetral.com)
 *
 * periodic timer API using timerfd functions 
 */

#include <sys/epoll.h>   /* epoll_ctl */
#include <utils.h>
#include <timer.h>

#define MAX_TIMERS 4

static timer_list_t timer_list;
static int fd_timer_service;
static int fd_epoll;

extern volatile uint32_t debug_flag;

static inline void dbg_timer(fsm_events_t evt_id, const char *msg)
{
	struct timespec ts;
	char buf[120];
	int len;

	if (!(debug_flag&DBG_TIMERS))
		return;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	len=snprintf(buf, sizeof(buf), "%s:%ld.%09ld %s\n",
		     evt_name[evt_id],
		     ts.tv_sec, ts.tv_nsec,
		     msg
		);

	write(1, buf, strlen(buf));
}

/**
 * find_timer_by_id - 
 *
 * @timerid: the unique timer id
 *
 * Return: a pointer to the fsmtimer_t
 */
fsmtimer_t *find_timer_by_id(uint32_t timerid)
{
	fsmtimer_t *timer_p;
	fsmtimer_t *found_p = NULL;
	pthread_mutex_lock(&timer_list.mutex);
	nl_list_for_each_entry(timer_p, &timer_list.head.list, list) {
		if (timerid == timer_p->timerid) {
			found_p = timer_p;
		}
	}
	
	pthread_mutex_unlock(&timer_list.mutex);
	return(found_p);
}

/**
 * find_timer_by_pollfd - 
 *
 * @pollfd: the fd from poll_wait
 *
 * Return: a pointer to the fsmtimer_t
 */
fsmtimer_t *find_timer_by_pollfd(int pollfd)
{
	fsmtimer_t* timer_p;
	fsmtimer_t *found_p = NULL;	
	pthread_mutex_lock(&timer_list.mutex);
	nl_list_for_each_entry(timer_p, &timer_list.head.list, list) {
		if (pollfd == timer_p->fd) {
			found_p = timer_p;
		}
	}
	pthread_mutex_unlock(&timer_list.mutex);
	return(found_p);
}

void show_timers(void)
{
	fsmtimer_t* timer_p;
	printf("timers\n%-2s:%-2s %-18s %-9s\n", "id", "fd", "event name", "msec val");
	pthread_mutex_lock(&timer_list.mutex);
	nl_list_for_each_entry(timer_p, &timer_list.head.list, list) {
		printf("%2u:%2d evt=%14s msec=%5lu\n", timer_p->timerid,
		       timer_p->fd,
		       evt_name[timer_p->evtid],
		       timer_p->tick_ms);
	}
	pthread_mutex_unlock(&timer_list.mutex);
}

/**
 * create_timer
 *
 * but don't start it
 */
int create_timer(uint32_t timerid, fsm_events_t evtid)
{
	fsmtimer_t *timer_p = malloc(sizeof(fsmtimer_t));
	struct epoll_event event;     /* struct to add to the epoll list */

	if (NULL != find_timer_by_id(timerid))
		die("timer exists");

	if (-1 == (timer_p->fd=timerfd_create(CLOCK_MONOTONIC, 0)))
		die("timerfd_create");

	timer_p->timerid = timerid;
	timer_p->evtid = evtid;
	timer_p->tick_ms = 0;

	pthread_mutex_lock(&timer_list.mutex);

	nl_list_add_tail(&timer_p->list, &timer_list.head.list);

	/* add to poll list */
	event.data.fd = timer_p->fd;
	event.events = EPOLLIN;
	if (-1 == epoll_ctl(fd_epoll, EPOLL_CTL_ADD, event.data.fd, &event))
		die("epoll_ctl for new timer");
	
	pthread_mutex_unlock(&timer_list.mutex);
	
	return(0);
}

/**
 * set_timer - set a timer to a new periodic timeout tick_ms in the future
 *
 * @timer_p: pointer to the timer structure
 * @tick_ms: a periodic trigger value in milliseconds
 *
 * Return: the tick_ms
 *
 * This is called from the timer thread to set, reset, cancel
 * a timer.  If ms == 0, the timer is cancelled.  If a running 
 * timer is being set, the future timeout is reset to this value.
 *
 * The current tick_ms is saved to be used by the toggle function 
 * below.
 */
int set_timer_p(fsmtimer_t *timer_p, uint64_t tick_ms)
{
	struct itimerspec ts;
	time_t sec;
	long nsec;

	if (NULL == timer_p)
		die("set_timer unknown timer");

	if (debug_flag & DBG_TIMERS) {
		printf("%d:%s set to %lu msecs\n",
		       timer_p->timerid,
		       evt_name[timer_p->evtid],
		       tick_ms);
	}

	/* save current tick before updating, used by toggle function */
	timer_p->old_tick_ms = timer_p->tick_ms;
	timer_p->tick_ms = tick_ms;
	if (debug_flag & DBG_TIMERS)
		printf("%d: old=%ld tick=%ld\n", timer_p->timerid,
		       timer_p->old_tick_ms, timer_p->tick_ms);
	
	/* convert ms into timerfd argument 
	 * special case for 0, which disarms the timer
	 */
	sec = tick_ms ? (tick_ms/1000) : 0;
	nsec = tick_ms ? (tick_ms%1000)*1e6 : 0;

	ts.it_value.tv_sec = sec;
	ts.it_value.tv_nsec = nsec;
	ts.it_interval.tv_sec = sec;
	ts.it_interval.tv_nsec = nsec;

	if (-1 == timerfd_settime(timer_p->fd, 0, &ts, NULL))
		die("set_timer");


	return(0);
}

/**
 * set_timer - stop the timer from generating periodic timeouts
 *
 * @timerfd: the unique timer fd
 *
 * Fully stop the timer, use set_timer to start again.
 */
int set_timer(uint32_t timerid, uint64_t tick_ms)
{
	fsmtimer_t *timer_p = find_timer_by_id(timerid);

	if (NULL == timer_p)
		die("stop_timer unknown timer");
	set_timer_p(timer_p, tick_ms);
}

/**
 * get_timer - remaing time in msec
 * @timerid: unique timerid in timer list
 */
uint64_t get_timer(uint32_t timerid)
{
	fsmtimer_t *timer_p = find_timer_by_id(timerid);
	struct itimerspec ts;
	uint64_t msec;
	
	if (NULL == timer_p)
		die("get_timer unknown timer");
	
	if (-1 == timerfd_gettime(timer_p->fd,&ts))
		die("get_timer");

	/* convert timerfd to msec */	
	msec = ts.it_value.tv_sec * 1000L + ts.it_value.tv_nsec / 1e6;

	if (debug_flag & DBG_TIMERS) {
		printf("%d: remaining msec=%ld\n", timerid, msec);
	}
	return (msec);
}

/**
 * toggle_timer
 *
 */
int toggle_timer(uint32_t timerid)
{
	fsmtimer_t *timer_p = find_timer_by_id(timerid);

	if (NULL == timer_p) {
		printf("%s: unknown timer %d\n", __func__, timerid);
		return (-1);
	}
	
	if (0 != timer_p->tick_ms) {
		dbg_timer(timer_p->timerid, "timer off");
		set_timer_p(timer_p, 0);
	} else {
		dbg_timer(timer_p->timerid, "timer restore");		
		set_timer_p(timer_p, timer_p->old_tick_ms);
	}

	return(0);
}

/**
 * timer_service_fn - pthread generating timer events to consumer
 * @arg: event queue array created by controlling thread
 *
 * - use epoll_wait with a short timeout to wait on timer expiration
 * 
 * thread loops forever until program exits or a pthread_cancel is sent
 * to it.
 */
void *timer_service_fn(void *arg)
{
	fsm_events_t evt_id;
	struct epoll_event events[MAX_TIMERS];
	uint64_t res;

	/* init the timer list */
	pthread_mutex_init(&timer_list.mutex, NULL);
	NL_INIT_LIST_HEAD(&timer_list.head.list);

	/* create epoll */
	if (-1 == (fd_epoll=epoll_create1(0)))
		die("epoll");
	
	if (0 != pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL))
		die("pthread_setcancelstate");
	
	while (1) {
		int nfds; /* number of ready file descriptors */

		/* set timeout to 200ms because workers may create_timer */
		nfds=epoll_wait(fd_epoll, events, MAX_TIMERS, 200);

		if (debug_flag & DBG_DEEP)
			printf("timer poll_wait fds=%d\n", nfds);

		switch(nfds) {
		case -1:
			/* if poll returns due to an signal interrupt then do nothing 
			 * otherwise die
			 */
			if (errno != EINTR)
				die("epoll_wait");
			break;
		case 0: /* just loop */
			break;
		default:
		{
			int i;
			fsmtimer_t *timer_p;
			
			for (i=0; i<nfds; i++) {
				/* bad event or corrupted file descriptor */
				if (!(events[i].events&EPOLLIN))
					die("bad incoming event");

				timer_p = find_timer_by_pollfd(events[i].data.fd);
				if (timer_p) {
					read(events[i].data.fd, &res, sizeof(res));	
					dbg_timer(timer_p->evtid, "expire");
					workers_evt_broadcast(timer_p->evtid);
				} else {
					die("unknown timer in poll list");
				}
			}
		}
		break;
		} /* switch */
	} /* while */
}
