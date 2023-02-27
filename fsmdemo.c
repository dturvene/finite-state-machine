/*
 * SPDX-License-Identifier: GPL-2.0
 * Copyright (C) 2021 Dahetral Systems
 * Author: David Turvene (dturvene@dahetral.com)
 *
 *
 * regression test:
 *  ./fsmdemo -n -s fsmdemo.script
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
#include "utils.h"
#include "evtq.h"
#include "fsm.h"
#include "workers.h"

#include <fsm_defs.h>

/**
 * arguments - descriptive string for all commandline arguments
 */
char *arguments = "\n"							\
	" -t tick: timer tick in msec\n"				\
	" -s scriptfile: read events from file\n"			\
	" -n: non-interactive mode (only read from scriptfile)\n"	\
	" -d hex: set debug_flag to hex level\n"			\
	"    0x01: debug FSM transitions\n"				\
	"    0x02: debug event push/pop\n"				\
	"    0x04: debug timers\n"					\
	"    0x10: debug FSM workers\n"					\
	"    0x20: debug deep for unit debug\n"				\
	" -h: this help\n";

/**
 *
 * tick - a base timer value multiplied to all timers (see the timer service)
 *
 */
uint32_t tick = 1000;

/**
 * scriptfile - default file name for event script to inject events
 * into consumer.
 * use the '-s filename' option to override
 */
char scriptfile[64] = "./fsmdemo.script";

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
 * fsm_task - archetype event consumer thread
 * @arg: worker_t context
 *
 * This is the generic FSM task.  It's a simple infinite loop that
 * - dequeues an event enqueued from another thread (or possibly this thread)
 * - injects the event into the FSM
 * All context persists in the worker_t instance.
 */
void *fsm_task(void *arg)
{
	worker_t* self_p = (worker_t*) arg;
        fsm_events_t evt_id;

	/* init the FSM and call the the init state enter functiuon */
	fsm_init(self_p->fsm_p);

	/* The main lupe
	 * dequeue event and call dbg_evts for runtime dump
	 * fsm_run for the fsm instance, injecting evt_id
	 *
	 * This is an infinite loop, either ^C (SIGINT) or
	 * E_DONE event will cause the FSM to call pthread_exit
	 */
	while (true)
	{
		evtq_dequeue(self_p->evtq_p, &evt_id);
		dbg_evts(evt_id);
		fsm_run(self_p->fsm_p, evt_id);
	}
	
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
 * @argv: list of arguments, passed to cmdlin_args() all remaining args are 
 *        discarded.
 * 
 * - process command line arguments
 * - set signal handlers (just in case)
 * - start timer service thread
 * - create a worker list
 * - create the worker pthread(s) and add to worker list
 * - call the evt_script | evt_producer function from the main thread
 * - cancel timer service
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

	/* create timer service and start it running */
	if (0 != pthread_create(&timer_service, NULL, timer_service_fn, NULL))
		die("timer_service create");

	worker_list_create();
	worker_list_add(worker_fsm_create(&fsm_task, "stoplight", FSM1));
	worker_list_add(worker_fsm_create(&fsm_task, "crosswalk", FSM2));

	/* loop until 'x' entered */
	non_interactive ? evt_script() : evt_producer();

	/* cancel timer_service thread */
	dbg("cancel timer_service and join");
	pthread_cancel(timer_service);
	pthread_join(timer_service, NULL);
	
	dbg("waiting for worker joins");
	join_workers();
	workers_evtq_destroy();

	dbg("exitting...\n");
}

