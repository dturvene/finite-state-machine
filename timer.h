/*
 * SPDX-License-Identifier: GPL-2.0
 * Copyright (C) 2005-2021 Dahetral Systems
 * Author: David Turvene (dturvene@dahetral.com)
 *
 * timerfd wrappers
 */

#ifndef _TIMER_H
#define _TIMER_H

#include <stdbool.h>     /* bool type and true, false values */
#include <inttypes.h>    /* include stdint.h, PRI macros, integer conversions */
#include <unistd.h>      /* write */
#include <stdlib.h>
#include <stdio.h>
#include <sys/timerfd.h>

extern int create_timer(void);
extern int set_timer(int timerfd, uint64_t tick_ms);
extern void stop_timer(int timerfd);
extern uint64_t get_timer(int timerfd);
	
#endif /* _TIMER_H */


