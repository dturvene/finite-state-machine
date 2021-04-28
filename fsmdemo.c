/*
 * SPDX-License-Identifier: GPL-2.0
 * Copyright (C) 2021 Dahetral Systems
 * Author: David Turvene (dturvene@dahetral.com)
 *
 * FSM
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
#include <fsm.h>
#include <workers.h>

/**
 * cleanly exit event loop 
 */
bool event_loop_done = false;

/* TODO move from global */
int fd_timer;                 /* timerfd reference */


/* max number of epoll events to wait for */
#define MAX_WAIT_EVENTS 1

/**
 * arguments - descriptive string for all commandline arguments
 */
char *arguments = "\n"							\
	" -s scriptfile: read events from file\n"			\
	" -n: non-interactive mode (only read from scriptfile)\n"	\
	" -d: set debug_flag\n"						\
	" -h: this help\n";

/**
 * scriptfile - file name for event script to inject events
 * into consumer.
 */
char scriptfile[64] = "./evtdemo.script";

/**
 * debug_flag - set the internal debug flag to print more info
 */
int debug_flag = 0;

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
	
	while((opt = getopt(argc, argv, "s:ndh")) != -1) {
		switch(opt) {
		case 's':
			strncpy(scriptfile, optarg, sizeof(scriptfile)-1);
			printf("Setting %s to %s\n", scriptfile, optarg);
		
			break;
		case 'n':
			non_interactive = true;
			break;
		case 'd':
			debug_flag = 1;
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
	sprintf(msg, "\nCatch %s, set event_loop_done\n", strsignal(sig));
	write(1, msg, strlen(msg));
	event_loop_done = true;
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
 * timer_task - pthread generating timer events to consumer
 * @arg: event queue array created by controlling thread
 *
 * - create a periodic timer and set it to 1sec interval
 * - use epoll_wait with a short timeout to wait on timer expiration
 * - if epoll timeouts, check input queue
 * - if periodic timer expires, send EVT_TIMER to consumer
 * - this task receives all events but discards EVT_TIMER
 */
void *timer_task(void *arg)
{
	workers_t* workers_p = (workers_t *) arg;
	fsm_trans_t *fsm_p = worker_self()->fsm;
	fsm_state_t *currst_p;
	fsm_state_t *nextst_p;
	evtq_t *evtq_self = worker_self()->evtq_p;
	
	fsm_events_t evt_id;
	bool done = false;

	int fd_epoll;                 /* epoll reference */
	
	uint64_t res;
	struct epoll_event event;     /* struct to add to the epoll list */
	struct epoll_event events[1]; /* struct return from epoll_wait */

	printf("%lu: %s\n", pthread_self(), __func__);

	/* create epoll */
	if (-1 == (fd_epoll=epoll_create1(0)))
		die("epoll");
	
	/* create an fd_timer and add to epoll list */
	event.data.fd = fd_timer = create_timer();
	event.events = EPOLLIN;
	if (-1 == epoll_ctl(fd_epoll, EPOLL_CTL_ADD, event.data.fd, &event))
		die("epoll_ctl for timerfd");

	/* init timer FSM */
	currst_p = fsm_p->curr;
	if (currst_p->entry_action)
		currst_p->entry_action(currst_p);

	while (EVT_DONE != evt_id) {
		int nfds; /* number of ready file descriptors */

		/* set timeout to 200ms to check event queue */
		nfds=epoll_wait(fd_epoll, events, MAX_WAIT_EVENTS, 200);

		switch(nfds) {
		case -1:
			/* if poll returns due to an signal interrupt then do nothing 
			 * otherwise die
			 */
			if (errno != EINTR)
				die("epoll_wait");
			break;
		case 0:
		{
			if (evtq_len(evtq_self) > 0)
			{
				evtq_pop(evtq_self, &evt_id);
				nextst_p = next_state(currst_p, evt_id, fsm_p);
				if (nextst_p) {
					if (currst_p->exit_action) {
						currst_p->exit_action(currst_p);
					}
					currst_p = nextst_p;
					if (currst_p->entry_action)
						currst_p->entry_action(currst_p);
				}
			} /* if */
		}
		break;
		default:
		{
			int i;
			for (i=0; i<nfds; i++) {
				/* bad event or corrupted file descriptor */
				if (!(events[i].events&EPOLLIN))
					die("bad incoming event");

				if (events[i].data.fd == fd_timer) {
					char msg[64];
					
					read(fd_timer, &res, sizeof(res));
					workers_evt_push(EVT_TIMER);
				}
			}
		}
		break;
		} /* switch */
	} /* while */
	
	dbg("exitting...");	
}

/************************* FSM data and tasks **********************/

void act_enter(void *arg)
{
	fsm_state_t *state_p = (fsm_state_t*) arg;
	printf("%ld %s: %s\n", pthread_self(), __func__, state_p->name);
}

void act_exit(void *arg)
{
	fsm_state_t *state_p = (fsm_state_t*) arg;
	printf("%ld %s: %s\n", pthread_self(), __func__, state_p->name);
}

void act_t3(void *arg)
{
	fsm_state_t *state_p = (fsm_state_t*) arg;
	printf("%s: %s\n", __func__, state_p->name);

	set_timer(fd_timer, 3000);
}

fsm_state_t st_init = {"INIT", act_enter, act_exit};
fsm_state_t st_done = {"DONE", act_enter, act_exit};
fsm_state_t st_red = {"RED", act_enter, act_exit};
fsm_state_t st_green = {"GREEN", act_enter, act_exit};
fsm_state_t st_yellow = {"YELLOW", act_enter, act_exit};

fsm_state_t st_timer_init = {"T3 INIT", act_t3, act_exit};
fsm_state_t st_run1 = {"RUN1", act_enter, act_exit};

fsm_trans_t FSM1[] = {
	{&st_init, EVT_INIT, &st_red},
	{&st_red, EVT_TIMER, &st_green},
	{&st_red, EVT_DONE, &st_done},
	{&st_green, EVT_TIMER, &st_yellow},
	{&st_green, EVT_DONE, &st_done},	
	{&st_yellow, EVT_TIMER, &st_red},
	{&st_yellow, EVT_DONE, &st_done},	
};

fsm_trans_t FSM3[] = {
	{&st_timer_init, EVT_INIT, &st_run1},
	{&st_run1, EVT_TIMER, &st_run1},
	{&st_run1, EVT_DONE, &st_run1},	
};
	
/**
 * fsm
 */
void *fsm_task(void *arg)
{
	workers_t* workers_p = (workers_t *) arg;
	fsm_trans_t *fsm_p = worker_self()->fsm;
	fsm_state_t *currst_p;
	fsm_state_t *nextst_p;
	
	evtq_t *evtq_self = worker_self()->evtq_p;
        fsm_events_t evt_id;

	printf("%lu: %s\n", pthread_self(), __func__);		

	currst_p = fsm_p->curr;
	if (currst_p->entry_action)
		currst_p->entry_action(currst_p);

	while (evt_id != EVT_DONE)
	{
		/* blocks until event is in queue */
		evtq_pop(evtq_self, &evt_id);
		nextst_p = next_state(currst_p, evt_id, fsm_p);
		if (nextst_p) {
			if (currst_p->exit_action) {
				currst_p->exit_action(currst_p);
			}
			currst_p = nextst_p;
			if (currst_p->entry_action)
				currst_p->entry_action(currst_p);
		}
	} /* while */
	
	dbg("exitting...");
}

/**
 * evt_producer - event production code to queue to evt_consumer
 * @arg: event queue created by controlling thread
 *
 * This machine has several notable mechanisms. The main one is a 
 * man:epoll loop to handle input from several sources:
 * - epoll error: many error types but this will exit when a signal is received 
 *   (which we ignore for SIGINT handling)
 * - periodic timer: timer expiration when no fds are ready
 * - fd=STDIN: on-demand input from user, call to evt_ondemand() to process
 *
 * The loop will exit when the global event_loop_done is true, which can occur 
 * either from a signal handler or evt_ondemand() input.
 * 
 */
void evt_producer(void)
{
	int fd_epoll;                 /* epoll reference */
	struct epoll_event event;     /* struct to add to the epoll list */
	struct epoll_event events[MAX_WAIT_EVENTS]; /* struct return from epoll_wait */

	/* create epoll */
	if (-1 == (fd_epoll=epoll_create1(0)))
		die("epoll");

	/* add stdin to epoll for user control */
	event.data.fd = STDIN_FILENO;
	event.events = EPOLLIN;
	if (-1 == epoll_ctl(fd_epoll, EPOLL_CTL_ADD, event.data.fd, &event))
		die("epoll_ctl for STDIN");

	/* event loop */
	printf("Enter event, 'h' for help, 'x' to exit\n");
	fflush(stdout);
	while (!event_loop_done) {
		int nfds; /* number of ready file descriptors */

		if (debug_flag)
			dbg("poll_wait");
		
		/* wait for desciptors or timeout 
		 * n < 0: error
		 * n == 0: timeout arg in ms, -1 no timeout
		 * n > 0: number of descriptors with pending I/O
		 */
		nfds=epoll_wait(fd_epoll, events, MAX_WAIT_EVENTS, -1);

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
					if (debug_flag)
						printf("\nread %d: %s\n", len, buf);

					// evt_ondemand(buf[0], evtq_pp);
					evt_ondemand(buf[0]);
				}
			}
		}
		break;
		} /* switch */
	} /* while */	
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

	// pthread_t task[2];
	// evtq_t *evtq_pp[2];

	parsed_args = cmdline_args(argc, argv);

	/* show commandline arguments that are NOT handled
	 * by cmdline_args.
	 */
	for (int i=parsed_args; i<argc; i++) {
		printf("%d: %s\n", i, argv[i]);
	}
	set_sig_handlers();

	worker_list_create();
	worker_list_add(fsm_create(&fsm_task, "stoplight", FSM1));
#if 0
	worker_list_add(worker_create(&timer_task, "timer"));
#else
	worker_list_add(fsm_create(&timer_task, "timer", FSM3));
#endif
	// show_workers();

	/* start your FSM! */
	workers_evt_push(EVT_INIT);

	non_interactive ? evt_script() : evt_producer();

	/* if interrupted MGMT may not have sent the critical EVT_DONE 
	 * to workers so do now.
	 */
	workers_evt_push(EVT_DONE);
	
	dbg("waiting for joins");
	join_workers();

	dbg("cleanup");
	// evtq_destroy_all(evtq_pp);

	dbg("exitting...");
}

