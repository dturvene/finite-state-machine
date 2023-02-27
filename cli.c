/*
 * SPDX-License-Identifier: GPL-2.0
 * Copyright (C) 2021 Dahetral Systems
 * Author: David Turvene (dturvene@dahetral.com)
 *
 * command line interface for the FSM project
 */

#include <sys/epoll.h>   /* epoll_ctl */

#include "utils.h"
#include "timer.h"
#include "evtq.h"
#include "workers.h"

/* default or set in the program arguments */
extern char scriptfile[];

/* default or set in the program arguments */
extern uint32_t tick;

/* max number of epoll events to wait for */
#define MAX_WAIT_EVENTS 1

/**
 * evt_script - load events from a file to added to event queue
 *
 * The input file is reference by the global scriptfile var.
 * 
 * This is just a shell to read a file for symbolic events
 */
void evt_script(void)
{
	FILE *fin;
	int len;
	char buf[120];
	
	if (NULL == (fin=fopen(scriptfile, "r")))
		die("unknown fname");

	while (NULL != fgets(buf, sizeof(buf), fin)) {

		/* skip empty lines */
		if (buf[0] == '\n')
			continue;

		/* print comments to output */
		if (buf[0] == '#') {
			printf("COMMENT:%s", buf);
			fflush(stdout);
			continue;
		}

		len = strlen(buf);
		
		if (debug_flag & DBG_DEEP)
			printf("%s: len=%d buf=%s", __func__, len, buf);

		/* call event parser */
		evt_parse_buf(buf);
	}

	fclose(fin);
}

/**
 * evt_parse_buf - translate symbolic event string to events and push to all worker
 * event queues.
 *
 * @buf: the symbolic event string
 * 
 * Each input event can be symbolically represented as a one or more characters.  
 * This function maps the chars to an internal event id (defined in evtq.h) and
 * pushes it to all workers.
 */
int evt_parse_buf(const char *buf)
{
	int ret = 0;
	const char *sp = buf;

	while (*sp) {
		if (isalnum(*sp)) {
			switch(*sp) {
			case 'h':
				printf("\tx,q: exit producer and workers (gracefully)\n");
				printf("\tw: show workers and curr state\n");
				printf("\tb: crosswalk button push\n");
				printf("\tg: go %s\n", evt_name[E_INIT]);
				printf("\teN: send event id N\n");
				printf("\tf: set timer fast\n");
				printf("\ttN: toggle timer N\n");
				printf("\tr: run event input script %s\n", scriptfile);
				printf("\ts: show current FSM state\n");
				printf("\tnN: main thread nap N ticks\n"
				      "(worker/timer threads keep running)\n");
				printf("\tp: pause CLI thread\n");
				printf("\tdefault: unknown command\n");
				break;
			case 'x':
			case 'q':
				/* exit event threads and main */
				workers_evt_broadcast(E_DONE);
				ret = 1;
				break;
			case 'w':
				show_workers();
				break;
			case 'g':
				workers_evt_broadcast(E_INIT);
				break;
			case 'f':
			{
				uint64_t msec = get_msec(2);
				
				if (msec == 500)
					set_timer(2, 2000);
				else if (msec == 2000)
					set_timer(2, 500);
				else
					printf("fast 2: msec = %ld\n", msec);
			}
			break;
			case 'b':
				workers_evt_broadcast(E_BUTTON);
				break;
			case 's':
				printf("*** FSM status\n");
				show_timers();
				show_workers();
				printf("*** END FSM status\n");
				break;
			case 'e':
			{
				/* get next char and convert to int */
				uint32_t evtid = (uint32_t)(*++sp - 0x30);
				workers_evt_broadcast(evtid);
			}
			break;
			case 't':
			{
				/* get next char and convert to int */
				uint32_t timerid = (uint32_t)(*++sp - 0x30);
				toggle_timer(timerid);
			}
			break;
			case 'r':
				evt_script();
				break;
			case 'n':
			{
				/* get next char and convert to int */
				uint32_t len = (uint32_t)(*++sp - 0x30);
				dbg_verbose("begin nap");
				nap(len*tick);
				dbg_verbose("after nap");
			}
			break;
			case 'p':
				relax();
				break;
			default:
				printf("%c: unknown cmd\n", *sp);
				break;
			} /* switch */
		} /* if isalnum */
		sp++;  /* next char */
	} /* while */

	return(ret);
}


/**
 * evt_producer - event producer to queue to workers
 *
 * This function has several notable mechanisms. The main one is a 
 * man:epoll loop to handle input from several sources:
 * - epoll error: many error types but this will exit when a signal is received 
 *   (which we ignore for SIGINT handling)
 * - fd=STDIN: on-demand input from user, call to evt_ondemand() to process
 *
 * The loop will exit when 'x' is entered
 * 
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
	printf("%s: Enter commands (g:start FSMs, h:help, x:exit)\n", __func__);
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
					char buf[32];
					int len;
					
					/* line buffered by tty driver so must hit CR to read */
					len=read(events[i].data.fd, buf, sizeof(buf));
					/* replace CR with string termination */
					buf[len] = '\0';
					if (debug_flag & DBG_DEEP)
						printf("\nread %d: %s\n", len, buf);

					done = evt_parse_buf(buf);
				}
			}
		}
		break;
		} /* switch */
	} /* while */

	dbg("exitting...");
}
