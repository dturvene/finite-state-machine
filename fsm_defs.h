/*
 * SPDX-License-Identifier: GPL-2.0
 * Copyright (C) 2021 Dahetral Systems
 * Author: David Turvene (dturvene@dahetral.com)
 *
 * 
 */

#ifndef _FSM_DEFS_H
#define _FSM_DEFS_H

#include <fsm.h>
#include <workers.h>

/*
 * action debug macro
 */
#define ACT_TRACE() do { \
		if (debug_flag & DBG_DEEP) {				\
			fsm_state_t *state_p = (fsm_state_t*) arg;	\
			printf("%s:%s %s\n", worker_get_name(), __func__, state_p->name); \
		}							\
	} while(0);

/* BUTTON press expiry */
#define TIMER_TBUT 250UL

/* YELLOW expiry */
#define TIMER_T3 500UL

/* RED, GREEN expiry */
#define TIMER_T1 10000UL

/************************************** FSM action functions *****************************/
void act_enter(void *arg)
{
	ACT_TRACE();
}

void act_exit(void *arg)
{
	ACT_TRACE();
}

extern int fd_timer;
void act_timer_norm(void *arg)
{
	ACT_TRACE();
	set_timer(fd_timer, TIMER_T3);
}

/*
 * act_timer_short
 *
 * get remaining time on timer in msec.  If larger than TIMER_T1,
 * reset timer to use TIMER_T1.  Otherwise just let it expire.
 */
void act_timer_short(void *arg)
{
	uint64_t rem;
	
	ACT_TRACE();
	rem = get_timer(fd_timer);
	if (debug_flag & DBG_TIMERS)
		printf("timer rem %lu\n", rem);
		
	if (rem > TIMER_T1)
		set_timer(fd_timer, TIMER_T1);
}

/*
 * act_tbut
 *
 * get remaining time on timer in msec.  If larger than TIMER_TBUT,
 * reset timer to use TIMER_TBUT.  Otherwise just let it expire.
 */
void act_tbut(void *arg)
{
	uint64_t rem;
	
	ACT_TRACE();
	rem = get_timer(fd_timer);
	if (debug_flag & DBG_TIMERS)
		printf("timer rem %lu\n", rem);

	/* if FSM2 is st_nowalk */
	if (rem > TIMER_TBUT)
		set_timer(fd_timer, TIMER_TBUT);
}

void act_green(void *arg)
{
	ACT_TRACE();
	workers_evt_broadcast(EVT_GREEN);
}

void act_yellow(void *arg)
{
	ACT_TRACE();
	workers_evt_broadcast(EVT_YELLOW);
}

void act_red(void *arg)
{
	ACT_TRACE();
	workers_evt_broadcast(EVT_RED);
}

void act_done(void *arg)
{
	ACT_TRACE();
	pthread_exit(NULL);
}

/********************************* FSM Definitions *******************************/

/* FSM1, stoplight */
fsm_state_t st_init = {"INIT", act_enter, act_exit};
fsm_state_t st_done = {"DONE", act_done, NULL};
fsm_state_t st_red = {"RED", act_red, act_exit};
fsm_state_t st_green = {"GREEN", act_green, act_exit};
fsm_state_t st_yellow = {"YELLOW", act_yellow, act_exit};
fsm_trans_t FSM1[] = {
	{&st_init, EVT_INIT, &st_red},

	{&st_red, EVT_TIMER, &st_green},
	{&st_red, EVT_DONE, &st_done},

	{&st_green, EVT_TIMER, &st_yellow},
	{&st_green, EVT_DONE, &st_done},	

	{&st_yellow, EVT_TIMER, &st_red},
	{&st_yellow, EVT_DONE, &st_done},	
};

/* FSM2, crosswalk */
fsm_state_t st_walk = {"WALK", act_enter, act_exit};
fsm_state_t st_blink = {"BLINKING WALK", act_enter, act_exit};
fsm_state_t st_nowalk = {"DONT WALK", act_enter, act_exit};
fsm_trans_t FSM2[] = {
	{&st_init, EVT_INIT, &st_init},
	{&st_init, EVT_RED, &st_walk},
	{&st_init, EVT_GREEN, &st_nowalk},
	
	{&st_walk, EVT_GREEN, &st_nowalk},
	{&st_walk, EVT_DONE, &st_done},
	 
	{&st_nowalk, EVT_RED, &st_walk},
	{&st_nowalk, EVT_DONE, &st_done},	
};

/* FSM3, timer event generator */
fsm_state_t st_timer_init = {"INIT", act_enter, act_exit};
fsm_state_t st_timer_norm = {"NORMAL", act_timer_norm, act_exit};
fsm_state_t st_timer_short = {"SHORT", act_timer_short, act_exit};
fsm_trans_t FSM3[] = {
	{&st_timer_init, EVT_INIT, &st_timer_norm},
	
	{&st_timer_norm, EVT_GREEN, &st_timer_short},
	{&st_timer_norm, EVT_BUTTON, &st_timer_short},
	{&st_timer_norm, EVT_DONE, &st_done},
	
	{&st_timer_short, EVT_RED, &st_timer_norm},
	{&st_timer_short, EVT_DONE, &st_done},
};

extern int fsm_run(worker_t* self_p, fsm_events_t evt_id);

#endif /* _FSM_DEFS_H */


