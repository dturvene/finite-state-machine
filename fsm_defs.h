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

/************************************ timer event ****************************************/
/* standard time TICK in msec */
#define TICK 1000UL

/* BUTTON press expiry */
#define T_BUT 1*TICK

/* YELLOW expiry */
#define T_FAST 3*TICK

/* RED, GREEN expiry */
#define T_NORM 10*TICK

/* time before entering BLINKING state */
#define T_BLINK T_NORM-2*TICK

/* timer ids used in create_timer, set_timer */
#define TID_LIGHT 1
#define TID_BLINK 2

/************************************** FSM action functions *****************************/
void act_enter(void *arg)
{
	ACT_TRACE();
}

void act_exit(void *arg)
{
	ACT_TRACE();
}

void act_done(void *arg)
{
	ACT_TRACE();
	pthread_exit(NULL);
}

void green_enter(void *arg)
{
	ACT_TRACE();
	workers_evt_broadcast(E_GREEN);
	set_timer(TID_LIGHT, T_NORM);
}

void yellow_enter(void *arg)
{
	ACT_TRACE();
	workers_evt_broadcast(E_YELLOW);
	set_timer(TID_LIGHT, T_FAST);
}

void red_enter(void *arg)
{
	ACT_TRACE();
	workers_evt_broadcast(E_RED);
	set_timer(TID_LIGHT, T_NORM);	
}

void green_but_enter(void *arg)
{
	ACT_TRACE();
	set_timer(TID_LIGHT, T_BUT);
}

bool but_constraint(void *arg)
{
	uint64_t rem;

	rem = get_timer(TID_LIGHT);
	if (rem > T_BUT)
		return(true);
	return(false);
}

void walk_enter(void *arg)
{
	ACT_TRACE();
	set_timer(TID_BLINK, T_BLINK);
}

void walk_exit(void *arg)
{
	ACT_TRACE();
	set_timer(TID_BLINK, 0);
}	
	

/********************************* FSM Definitions *******************************/

/* Common states */
fsm_state_t s_init = {"S_INIT", act_enter, act_exit};
fsm_state_t s_done = {"S_DONE", act_done, NULL};

/* FSM1, stoplight */
/* TODO: put timer create in S_INIT actions? */
fsm_state_t s_stoplight_init = {"S_INIT", act_enter, act_exit};
fsm_state_t s_red = {"S_RED", red_enter, act_exit};
fsm_state_t s_green = {"S_GREEN", green_enter, act_exit};
fsm_state_t s_yellow = {"S_YELLOW", yellow_enter, act_exit};
fsm_state_t s_green_but = {"S_GREEN_BUT", green_but_enter, act_exit};
fsm_trans_t FSM1[] = {
	{&s_stoplight_init, E_INIT, NULL, &s_green},

	/* GREEN */
	{&s_green, E_LIGHT, NULL, &s_yellow},
	{&s_green, E_DONE, NULL, &s_done},
	{&s_green, E_BUTTON, but_constraint, &s_green_but},

	/* YELLOW */
	{&s_yellow, E_LIGHT, NULL, &s_red},
	{&s_yellow, E_DONE, NULL, &s_done},	

	/* RED */
	{&s_red, E_LIGHT, NULL, &s_green},
	{&s_red, E_DONE, NULL, &s_done},

	/* GREEN BUT */
	{&s_green_but, E_LIGHT, NULL, &s_yellow},
	// NO DONE? {&st_green_but, E_DONE, &st_done},	

};

/* FSM2, crosswalk */
fsm_state_t s_nowalk = {"DONT WALK", act_enter, act_exit};
fsm_state_t s_walk = {"WALK", walk_enter, act_exit};
fsm_state_t s_blink = {"BLINKING WALK", act_enter, act_exit};
fsm_trans_t FSM2[] = {
	{&s_init, E_INIT, NULL, &s_nowalk},

	/* DONT WALK */
	{&s_nowalk, E_RED, NULL, &s_walk},
	{&s_nowalk, E_DONE, NULL, &s_done},	

	/* WALK */
	{&s_walk, E_BLINK, NULL, &s_blink},
	{&s_walk, E_DONE, NULL, &s_done},

	/* BLINKING */
	{&s_blink, E_GREEN, NULL, &s_nowalk},
	{&s_blink, E_DONE, NULL, &s_done},
};

#endif /* _FSM_DEFS_H */


