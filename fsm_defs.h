/*
 * SPDX-License-Identifier: GPL-2.0
 * Copyright (C) 2021 Dahetral Systems
 * Author: David Turvene (dturvene@dahetral.com)
 *
 * Definitions for FSM1 (stoplight) and FSM2 (crosswalk) described
 * in the README.md
 *
 *  
 */

#ifndef _FSM_DEFS_H
#define _FSM_DEFS_H

#include <timer.h>
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

/************************************ timers ****************************************/
extern uint32_t tick; /* forward definition */

#if 0
/* BUTTON press expiry */
#define T_BUT 1

/* YELLOW expiry */
#define T_FAST 3

/* RED, GREEN expiry */
#define T_NORM 10

/* time before entering BLINKING state */
#define T_BLINK (T_NORM-2)
#endif

/* timer ids used in create_timer, set_timer */
enum timer_ids {
	TID_LIGHT,
	TID_BLINK,
};


extern uint32_t tick;
uint32_t t_norm = 10;
uint32_t t_fast = 3;
uint32_t t_but = 1;
uint32_t t_blink = (10-2);

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

void stoplight_init_enter(void *arg)
{
	ACT_TRACE();

	/* create timers with event on expiry */
	create_timer(TID_LIGHT, E_LIGHT);
	create_timer(TID_BLINK, E_BLINK);

	/* update timer expiry periods to be adjustable */
	/* TODO: make timer periods more descriptive */
	t_norm *= tick;
	t_fast *= tick;	
	t_but *= tick;
	t_blink *= tick;
}

void green_enter(void *arg)
{
	ACT_TRACE();
	workers_evt_broadcast(E_GREEN);
	set_timer(TID_LIGHT, t_norm);
}

void yellow_enter(void *arg)
{
	ACT_TRACE();
	workers_evt_broadcast(E_YELLOW);
	set_timer(TID_LIGHT, t_fast);
}

void red_enter(void *arg)
{
	ACT_TRACE();
	workers_evt_broadcast(E_RED);
	set_timer(TID_LIGHT, t_norm);	
}

void green_but_enter(void *arg)
{
	ACT_TRACE();
	set_timer(TID_LIGHT, t_but);
}

bool but_constraint(void *arg)
{
	uint64_t rem;

	rem = get_timer(TID_LIGHT);
	if (rem > t_but)
		return(true);
	return(false);
}

void walk_enter(void *arg)
{
	ACT_TRACE();
	set_timer(TID_BLINK, t_blink);
}

void walk_exit(void *arg)
{
	ACT_TRACE();
	set_timer(t_blink, 0);
}	

/********************************* FSM Definitions *******************************/

/* Default states */
fsm_state_t s_init = {"S:INIT", act_enter, act_exit};
fsm_state_t s_done = {"S:DONE", act_done, NULL};

/* FSM1, stoplight */
fsm_state_t s_stoplight_init = {"S:INIT", stoplight_init_enter, act_exit};
fsm_state_t s_red = {"S:RED", red_enter, act_exit};
fsm_state_t s_green = {"S:GREEN", green_enter, act_exit};
fsm_state_t s_yellow = {"S:YELLOW", yellow_enter, act_exit};
fsm_state_t s_green_but = {"S:GREEN_BUT", green_but_enter, act_exit};
fsm_trans_t FSM1[] = {
	/* specific init for timers, transition to s_green */
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
fsm_state_t s_nowalk = {"S:DONT_WALK", act_enter, act_exit};
fsm_state_t s_walk = {"S:WALK", walk_enter, act_exit};
fsm_state_t s_blink = {"S:BLINKING WALK", act_enter, act_exit};
fsm_trans_t FSM2[] = {
	/* generic init to s_nowalk */
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


