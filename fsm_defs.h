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

/************************************ timers ****************************************/
/* timer ids used in create_timer, set_timer */
enum timer_ids {
	TID_LIGHT,
	TID_BLINK,
};

/*
 * Set timers relative to the tick commandline arg
 */
extern uint32_t tick;

/**
 * timers - values are all multipled by tick to speed up/down
 *  the state transitions for testing/regression.
 * t_norm: timeout for red/green lights
 * t_fast: timeout for yellow light
 * t_but: timeout after button push (see FSM1)
 * t_blink: timout for crosswalk to start blinking,
 *          indicating that it will soon change to DONT WALK.
 */
uint32_t t_norm = 10;
uint32_t t_fast = 3;
uint32_t t_but = 1;
uint32_t t_blink = (10-2);

/************************************** FSM action functions *****************************/

/**
 * These are action routines FSM states. See structures in `fsm.h`
 */

/*
 * act_enter - default state enter action, for debug
 */
static void act_enter(void *arg)
{
	ACT_TRACE();
}

/*
 * act_exit - default state exit action, for debug
 */
static void act_exit(void *arg)
{
	ACT_TRACE();
}

/**
 * act-done - when the E_DONE event is received, exit the thread immediately.  The main thread 
 * waits on pthread_join to reap the worker threads.
 */
static void act_done(void *arg)
{
	ACT_TRACE();
	pthread_exit(NULL);
}

/**
 * stoplight_init_enter - init stoplight FSM
 *
 * When FSMs are started with E_INIT event, each is responsible to provision
 * itself.  This creates two timers: TID_LIGHT for changing the stoplight
 * and TID_BLINK for crosswalk blinking.  A set of timeout values are configured
 * as increments of the command line argument `tick`.
 * - t_norm: normal timeout for light change
 * - t_fast: timeout for yellow light, which is brief
 * - t_but: timeout for light when button is pressed
 * - t_blink: crosswalk blinking when stoplight is getting near S:GREEN
 */
static void stoplight_init_enter(void *arg)
{
	ACT_TRACE();

	/* create timers with event on expiry */
	create_timer(TID_LIGHT, E_LIGHT);
	create_timer(TID_BLINK, E_BLINK);

	/* update timer expiry periods to be adjustable */
	t_norm *= tick;
	t_fast *= tick;	
	t_but *= tick;
	t_blink *= tick;
}

/**
 * green_enter - broadcast event and set light timer for normal
 * timeout.
 */
static void green_enter(void *arg)
{
	ACT_TRACE();
	workers_evt_broadcast(E_GREEN);
	set_timer(TID_LIGHT, t_norm);
}

/**
 * yellow_enter - broadcast event and set light timer for fast
 * timeout because yellow should be brief.
 */
static void yellow_enter(void *arg)
{
	ACT_TRACE();
	workers_evt_broadcast(E_YELLOW);
	set_timer(TID_LIGHT, t_fast);
}

/**
 * red_enter - broadcast event and set light timer for normal
 * timeout.
 */
static void red_enter(void *arg)
{
	ACT_TRACE();
	workers_evt_broadcast(E_RED);
	set_timer(TID_LIGHT, t_norm);	
}

/**
 * green_but_enter - action entering S:GREEN_BUT state to
 * update the light timer to t_but, which will cause RED/WALK more
 * quicker. 
 */
static void green_but_enter(void *arg)
{
	ACT_TRACE();
	set_timer(TID_LIGHT, t_but);
}

/**
 * walk_enter - set the walk signal blink timer to expire
 * before the stoplight turns from from RED to GREEN.  When the
 * timer expires, the crosswalk enters the S:BLINKING WALK state.
 */
static void walk_enter(void *arg)
{
	ACT_TRACE();
	set_timer(TID_BLINK, t_blink);
}

/*
 * walk-exit - clear the crosswalk blink timer
 */
static void walk_exit(void *arg)
{
	ACT_TRACE();
	set_timer(TID_BLINK, 0);
}

/**
 * but_constraint - a UML guard for trans to S:GREEN_BUT
 *
 * Return: true if constraint is satisfied, false otherwise
 * 
 * If the light timer remaining time is greater than the button time
 * return true, which will transition to S:GREEN_BUT IFF FSM is in S:GREEN
 * If the remaining time is smaller, fail the constraint (no transition.)
 */
static bool but_constraint(void *arg)
{
	uint64_t rem;

	rem = get_timer(TID_LIGHT);
	if (rem > t_but)
		return(true);
	return(false);
}


/********************************* FSM Definitions *******************************/

/* Default states */
fsm_state_t s_init = {"S:INIT", act_enter, act_exit};
fsm_state_t s_done = {"S:DONE", act_done, NULL};

/**
 * FSM1, stoplight 
 */
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
	// TODO: NO DONE? {&st_green_but, E_DONE, &st_done},

};

/**
 * FSM2, crosswalk 
 */
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


