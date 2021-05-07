/*
 * SPDX-License-Identifier: GPL-2.0
 * Copyright (C) 2021 Dahetral Systems
 * Author: David Turvene (dturvene@dahetral.com)
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
#include <utils.h>
#include <evtq.h>
#include <timer.h>
#include <fsm.h>
#include <fsm_defs.h>
#include <workers.h>

/**
 * arguments - descriptive string for all commandline arguments
 */
char *arguments = "\n"							\
	" -s scriptfile: read events from file\n"			\
	" -n: non-interactive mode (only read from scriptfile)\n"	\
	" -d level: set debug_flag to hex level\n"			\
	" -h: this help\n";

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
	int argcnt = 0;
	
	while((opt = getopt(argc, argv, "s:nd:h")) != -1) {
		switch(opt) {
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
 */
void *fsm_task(void *arg)
{
	worker_t* self_p = (worker_t*) arg;
        fsm_events_t evt_id;

	fsm_init(self_p->fsm_p);

	/* worker_self() loops through worker_list for match on pthread_self */
	while (true)
	{
		evtq_pop(self_p->evtq_p, &evt_id);
		dbg_evts(evt_id);
		fsm_run(self_p->fsm_p, evt_id);
	}
	
	dbg("exitting...");
}

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

	/* create timer service and start it running */
	if (0 != pthread_create(&timer_service, NULL, timer_service_fn, NULL))
		die("timer_service create");

	worker_list_create();
	worker_list_add(worker_fsm_create(&fsm_task, "stoplight", FSM1));
	worker_list_add(worker_fsm_create(&fsm_task, "crosswalk", FSM2));

	/* TODO: create timers in FSMs? */
	create_timer(TID_LIGHT, E_LIGHT);
	create_timer(TID_BLINK, E_BLINK);

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

