/*
 * SPDX-License-Identifier: GPL-2.0
 * Copyright (C) 2021 Dahetral Systems
 * Author: David Turvene (dturvene@dahetral.com)
 *
 * periodic timer API using timerfd functions 
 */

#include <utils.h>
#include <timer.h>

/**
 * create_timer - create and return the timerfd file descriptor
 *
 * Return: the file descriptor to be used in other timer routines
 *
 * The timerfd can be used in an epoll_wait loop to generate a periodic
 * timer function. This should be done only during initialization and
 * set_timer to manage it.
 */
int create_timer(void)
{
	int fd;
	
	if (-1 == (fd=timerfd_create(CLOCK_MONOTONIC, 0)))
		die("timerfd_create");

	return(fd);
}

/**
 * set_timer - set a timer to a new periodic timeout tick_ms in the future
 *
 * @timerfd: the unique timer fd
 * @tick_ms: a periodic trigger value in milliseconds

 * Return: the tick_ms
 *
 * This is called from the timer thread to set, reset, cancel
 * a timer.  If ms == 0, the timer is cancelled.  If a running 
 * timer is being set, the future timeout is reset to this value.
 */
int set_timer(int timerfd, uint64_t tick_ms)
{
	struct itimerspec ts;
	time_t sec;
	long nsec;

	if (debug_flag & DBG_TIMERS) {
		printf("timer set to %lu msecs\n", tick_ms);
	}

	/* convert ms into timerfd argument 
	 * special case for 0, which cancels the timer
	 */
	sec = tick_ms ? (tick_ms/1000) : 0;
	nsec = tick_ms ? (tick_ms%1000)*1e6 : 0;

	ts.it_value.tv_sec = sec;
	ts.it_value.tv_nsec = nsec;
	ts.it_interval.tv_sec = sec;
	ts.it_interval.tv_nsec = nsec;

	if (-1 == timerfd_settime(timerfd, 0, &ts, NULL))
		die("timerfd_settime");

	return tick_ms;
}

/**
 * stop_timer - stop the timer from generating periodic timeouts
 *
 * @timerfd: the unique timer fd
 *
 * Fully stop the timer, use set_timer to start again.
 */
void stop_timer(int timerfd)
{
	struct itimerspec ts = {0, 0, 0, 0};
	if (-1 == timerfd_settime(timerfd, 0, &ts, NULL))
		die("timerfd_settime");
}

/**
 * get_timer - return the current time left before timeout in ms
 *
 * @timerfd: the unique timer fd
 *
 */
uint64_t get_timer(int timerfd)
{
	struct itimerspec ts;
	
	if (-1 == timerfd_gettime(timerfd, &ts))
		die("timer_gettime");

	return (ts.it_value.tv_sec * 1000L + ts.it_value.tv_nsec / 1e6);
}
