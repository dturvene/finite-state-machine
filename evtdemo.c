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
#include <utils.h>
#include <evtq.h>
#include <timer.h>
#include <workers.h>

/* max number of epoll events to wait for */
#define MAX_WAIT_EVENTS 1

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
		case E_TMR_LIGHT:
			dbg("timer 2 (TMR_LIGHT) expiry");
			break;
		case E_TMR_CROSS:
			dbg("timer 3 (TMR_CROSS) expiry");
			break;
		case E_INIT:
			dbg("create 2 (E_TMR_LIGHT)");
			create_timer(2, E_TMR_LIGHT);
			dbg("set timer 2 TMR_LIGHT = 2000");
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
			dbg("set 3 TMR_CROSS = 1000");			
			set_timer(3, 1000);
			break;
#if 0			
		case E_TMR_LIGHT:
			break;
#endif
		case E_TMR_CROSS:
			dbg("timer 3 (TMR_CROSS) expiry");
			break;
		case E_INIT:
			dbg("create timer 3");
			create_timer(3, E_TMR_CROSS);
			break;
		case E_IDLE:
			nap(100);
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
 * TODO: copy 
 */
void evt_producer(void)
{
	int fd_epoll;                 /* epoll reference */
	struct epoll_event event;     /* struct to add to the epoll list */
	struct epoll_event events[MAX_WAIT_EVENTS]; /* struct return from epoll_wait */
	int done = 0;

	/* create epoll */
	if (-1 == (fd_epoll=epoll_create1(0)))
		die("epoll");

	/* add stdin to epoll for user control */
	event.data.fd = STDIN_FILENO;
	event.events = EPOLLIN;
	if (-1 == epoll_ctl(fd_epoll, EPOLL_CTL_ADD, event.data.fd, &event))
		die("epoll_ctl for STDIN");

	/* event loop */
	printf("%s: Enter event, 'h' for help, 'x' to exit\n", __func__);
	fflush(stdout);
	while (!done) {
		int nfds; /* number of ready file descriptors */

		dbg_verbose("poll_wait");
		
		/* wait for desciptors or timeout (in ms, -1 no timeout) */
		nfds=epoll_wait(fd_epoll, events, MAX_WAIT_EVENTS, -1);

		/* 
		 * nfds < 0: error
		 * nfds == 0: timeout
		 * nfds > 0: number of descriptors with pending I/O
		 */
		switch(nfds) {
		case -1:
			/* if poll returns due to an signal interrupt then do nothing 
			 * otherwise die
			 */
			if (errno != EINTR)
				die("epoll_wait");
			break;
		case 0:
			dbg("epoll timeout");
		break;
		default:
		{
			int i;
			for (i=0; i<nfds; i++) {
				/* bad event or corrupted file descriptor */
				if (!(events[i].events&EPOLLIN))
					die("bad incoming event");

				if (events[i].data.fd == STDIN_FILENO) {
					char buf[2];
					int len;
					
					/* line buffered by tty driver so must hit CR to read */
					len=read(events[i].data.fd, buf, sizeof(buf));
					/* replace CR with string termination */
					buf[len] = '\0';
					if (debug_flag & DBG_DEEP)
						printf("\nread %d: %s\n", len, buf);

					done = evt_ondemand(buf[0]);
				}
			}
		}
		break;
		} /* switch */
	} /* while */

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
	
	if (0 != pthread_create(&timer_service, NULL, timer_service_fn, NULL))
		die("timer_service create");

	worker_list_create();
	worker_list_add(worker_create_self(&evt_c1, "consumer1"));
	worker_list_add(worker_create_self(&evt_c2, "consumer2"));	

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

