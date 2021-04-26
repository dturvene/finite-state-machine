/*
 * SPDX-License-Identifier: GPL-2.0
 * Copyright (C) 2005-2021 Dahetral Systems
 * Author: David Turvene (dturvene@dahetral.com)
 *
 * A collection of support utils
 * 
 * die: test program will fail so exit with an error
 * nap: sleep for N milliseconds
 * relax: stop running the thread and put it at tail of run queue
 * dbg: function, timestamp, msg write to stdout
 */

#ifndef _UTILS_H
#define _UTILS_H

#include <inttypes.h>    /* include stdint.h, PRI macros, integer conversions */
#include <unistd.h>      /* write */
#include <stdio.h>       /* snprintf */
#include <sched.h>       /* sched_yield */
#include <time.h>        /* nanosleep, clock_gettime */
#include <string.h>      /* strlen */

/**
 * die - terminate task with a descriptive error message
 * @msg: null-terminated string prepended to system error
 *
 * This is a small helper macro to descriptive but suddenly 
 * exit when an unrecoverable situation occurs.
 */
#define die(msg) do { perror(msg); exit(EXIT_FAILURE); } while (0)

/**
 * nap - small sleep
 * @ms: number of msecs to nap
 *
 * useful for very small delays, causes thread switch
 */
inline static void nap(uint32_t ms)
{
	struct timespec t;

	t.tv_sec = (ms/1000);
	t.tv_nsec = (ms%1000)*1e6;
	nanosleep(&t, NULL);
}

/**
 * relax - calling thread is rescheduled so waiting threads can run
 * 
 * nap(1) performs a similar function.
 */
inline static void relax()
{
	sched_yield();
}

/* 
 * _dbg_func - dump debug info to stdout
 * @func: calling function
 * @msg: informational message string
 *
 * This will get current clock and create an information string containing
 * calling function, timestamp and information message, then write string to stdout.
 * Use write instead of printf so can be called from interrupt handlers.  printf has
 * an internal mutex that can cause deadlock. 
 */
inline static void _dbg_func(const char *func, const char *msg)
{
	struct timespec ts;
	char buff[64];
	
	clock_gettime(CLOCK_MONOTONIC, &ts);
	snprintf(buff, sizeof(buff)-1, "%s:%ld.%09ld %s\n", func, ts.tv_sec, ts.tv_nsec, msg);
	write(1, buff, strlen(buff));
}
#define dbg(msg) _dbg_func(__func__, msg);

#endif /* _UTILS_H */


