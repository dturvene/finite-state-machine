/*
 * SPDX-License-Identifier: GPL-2.0
 * Copyright (C) 2021 Dahetral Systems
 * Author: David Turvene (dturvene@dahetral.com)
 *
 * event producer/consumer
 * 
 * This is a pthread framework for reliable event generation.  It uses
 * a pthread mutex and cond to guard access to the event queue pushed
 * and popped by the threads.  A timer event is implemented in the 
 * producer via epoll_wait timeout.
 * 
 * Three threads: 
 * - main thread (producer)
 * - consumer thread 
 * - timer thread
 *
 * The main thread creates the consumer and timer threads and an event 
 * queue to each.

 * Next the main thread sits on a blocking epoll_wait to read symbolic
 * fsm_events from STDIN. It translates to internal events and sends to all 
 * fsm_event queues.  Symbolic events can also be read from a script 
 * file.  See evt_producer and evt_parse_buf for the logic.
 * When the producer sends an E_DONE, all threads will exit their respective  
 * event loops.
 *
 * The consumer thread event loop blocks on pthead_cond_wait until signalled that 
 * an fsm_event is present its event queue and prints the event.  It does 
 * not send any events.
 *
 * The timer thread sits on a timerfd and periodically sends a timer 
 * fsm_event to the consumer thread.  It has a poll_wait timeout to checks 
 * its fsm_event queue for incoming events.
 *
 */

#include <stdlib.h>      /* atoi, malloc, strtol, strtoll, strtoul */
#include <stdbool.h>     /* bool type and true, false values */
#include <inttypes.h>    /* include stdint.h, PRI macros, integer conversions */
#include <unistd.h>      /* common typdefs, e.g. ssize_t, includes getopt.h */
#include <errno.h>       /* perror, see /usr/include/asm-generic for E-codes */
#include <time.h>        /* clock_gettime, struct timespec, timer_create */
#include <stdio.h>       /* char I/O */
#include <signal.h>      /* sigaction */
#include <string.h>      /* strlen, strsignal,, memset */
#include <pthread.h>     /* posix threads */
#include <sys/epoll.h>   /* epoll_ctl */
#include "utils.h"
#include "evtq.h"
#include "timer.h"
#include "workers.h"

/* max number of epoll events to wait for */
#define MAX_WAIT_EVENTS 1

/**
 * arguments - descriptive string for all commandline arguments
 */
char *arguments = "\n"							\
	" -t tick: timer tick in msec\n"				\
	" -s scriptfile: read events from file\n"			\
	" -n: non-interactive mode (only read from scriptfile)\n"	\
	" -d level: set debug_flag to hex level\n"			\
	" -h: this help\n";

/**
 *
 * tick - a base timer value multiplied to all timers (see the timer service)
 *
 */
uint32_t tick = 1000;

/**
 * scriptfile - file name for event script to inject events
 * into consumer.
 */
char scriptfile[64] = "./evtdemo.script";

/**
 * non_interactive - If false then commands are accepted
 *  from STDIN in an epoll loop.  If true then the given scriptfile
 *  is read for input.  The script file must contain an 'x' event.                
 */
static bool non_interactive = false;

/**
 * debug_flag - bitmask for enabling levels of logging
 */
uint32_t debug_flag;

/**
 * cmdline_args - parse command line arguments
 * @argc: argument count (from main)
 * @argv: array aligned with argc containing an argument string (from main)
 *
 * This uses man:getopt to parse hyphenated commandline arguments.  Generally
 * arguments will set a static variable.  Unknown hyphenated arguments will exit.
 * Non-hyphenated arguments are not parsed, and should be handled by the caller.
 * 
 * Return:
 *  optind - the number of parsed (hyphenated) arguments
 */
int cmdline_args(int argc, char *argv[]) {
	int opt;
	
	while((opt = getopt(argc, argv, "t:s:nd:h")) != -1) {
		switch(opt) {
		case 't':
			tick = strtoul(optarg, NULL, 0);
			printf("Setting timer tick to %u\n", tick);
			break;
		case 's':
			strncpy(scriptfile, optarg, sizeof(scriptfile)-1);
			printf("Setting %s to %s\n", scriptfile, optarg);
		
			break;
		case 'n':
			non_interactive = true;
			break;
		case 'd':
			debug_flag = strtoul(optarg, NULL, 0);
			break;
		case 'h':
		default:			
			fprintf(stderr, "Usage: %s %s\n", argv[0], arguments);
			exit(0);
		}
	}
	return optind;
}

/********************** signal handling section **********************/

/**
 * sig_handler - placeholder for SIGINT
 * @sig: signal number (man7:signal)
 *
 * Catch the signal, write to stdout and exit.  This cannot use
 * printf due to the internal mutex locking.
 * 
 * This must conform to the struct sigaction prototype as defined
 * for sa_handler (man:sigaction).
 */
void sig_handler(int sig) {
	char msg[80];
	sprintf(msg, "\nCatch %s and exit\n", strsignal(sig));
	write(1, msg, strlen(msg));
	exit(0);
}

/**
 * set_sig_handlers - set up the default signal handlers
 *
 * SIGINT uses the old sa_handler action for simplicity.
 */
void set_sig_handlers(void) {
	struct sigaction sa;

	sigemptyset(&sa.sa_mask);
	sa.sa_handler = &sig_handler;
	sa.sa_flags = 0;
	if (-1 == sigaction(SIGINT, &sa, NULL))
		die("sigint");
}

/********************************** application logic *******************************/

/**
 * evt_c1 - archetype event consumer thread
 * @arg: event queue array created by controlling thread
 *
 * This is a simple framework:
 * - dequeue an event when it is available (evtq_dequeue blocks until then)
 * - print the event
 * - if it is an E_DONE event end loop and exit
 */
void *evt_c1(void *arg)
{
	worker_t* self_p = (worker_t *) arg;
	evtq_t *evtq_self;
        fsm_events_t evt_id;

	dbg("enter and wait for fsm events");

	/* worker_self() loops through worker_list for match on pthread_self */
	evtq_self = self_p->evtq_p;
	while (true)
	{
		evtq_dequeue(evtq_self, &evt_id);
		dbg_evts(evt_id);
		
		switch(evt_id) {
		case E_TIMER:
			break;
		case E_LIGHT:
			dbg("timer 2 (E_LIGHT) expiry");
			break;
		case E_BLINK:
			dbg("timer 3 (E_BLINK) expiry");
			break;
		case E_INIT:
			dbg("create 2 (E_LIGHT)");
			create_timer(2, E_LIGHT);
			dbg("set timer 2 2000");
			set_timer(2, 2000);		
			break;
		case E_DONE:
			pthread_exit(NULL);
			break;
		default:
			break;
		} /* switch */
	} /* while */
	
	dbg("exitting...");
}

/**
 * evt_c2 - archetype event consumer thread
 * @arg: event queue array created by controlling thread
 *
 * This is a simple framework:
 * - dequeue an event when it is available (evtq_dequeue blocks until then)
 * - print the event
 * - if it is an E_DONE event end loop and exit
 */
void *evt_c2(void *arg)
{
	worker_t* self_p = (worker_t *) arg;
	evtq_t *evtq_self;
        fsm_events_t evt_id;

	dbg("enter and wait for fsm events");

	/* worker_self() loops through worker_list for match on pthread_self */
	evtq_self = self_p->evtq_p;
	while (true)
	{
		evtq_dequeue(evtq_self, &evt_id);
		dbg_evts(evt_id);
		
		switch(evt_id) {
		case E_TIMER:
			dbg("set 3 E_BLINK 1000");			
			set_timer(3, 1000);
			break;
		case E_BLINK:
			dbg("timer 3 expiry");
			break;
		case E_INIT:
			dbg("create timer 3");
			create_timer(3, E_BLINK);
			break;
		case E_DONE:
			pthread_exit(NULL);
			break;
		default:
			break;
		} /* switch */
	} /* while */
	
	dbg("exitting...");
}

/*
 * workers - global linked list of worker threads.  See workers.h
 */
workers_t workers;

/**
 * main - a simple driver for an event producer/consumer framework (MGMT)
 *
 * @argc: argument count, this will be decremented by cmdline_args()
 * @argv: list of arguments, passed to cmdlin_args() and the printed and discarded.
 * 
 * - process command line arguments
 * - set signal handlers (just in case)
 * - create an event queue for the consumer
 * - create the consumer pthread
 * - call the evt_producer function from the main thread
 * - wait for consumer thread to terminate
 * - destroy event_queue for the consumer
 */
int main(int argc, char *argv[])
{
	int parsed_args;
	pthread_t timer_service;

	parsed_args = cmdline_args(argc, argv);

	/* show commandline arguments that are NOT handled
	 * by cmdline_args.
	 */
	for (int i=parsed_args; i<argc; i++) {
		printf("%d: %s\n", i, argv[i]);
	}

	/* all threads in process use this */
	set_sig_handlers();
	
	if (0 != pthread_create(&timer_service, NULL, timer_service_fn, NULL))
		die("timer_service create");

	worker_list_create();
	worker_list_add(worker_create(&evt_c1, "consumer1"));
	worker_list_add(worker_create(&evt_c2, "consumer2"));

	/* create a test timer */

	/* loop until 'x' entered */
	non_interactive ? evt_script() : evt_producer();

	/* cancel timer_service thread */
	dbg("cancel timer_service and join\n");
	pthread_cancel(timer_service);
	pthread_join(timer_service, NULL);
	
	dbg("waiting for worker joins\n");
	join_workers();
	workers_evtq_destroy();

	dbg("exitting...\n");
}

