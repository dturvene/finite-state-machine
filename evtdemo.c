/*
 * event producer/consumer
 * 
 * This is a pthread framework for reliable event generation.  It uses
 * a pthread mutex and cond to guard access to the event queue pushed
 * and popped by the threads.  A timer event is implemented in the 
 * producer via epoll_wait timeout.
 * 
 * Two threads: 
 * - main thread (producer)
 * - consumer thread 
 *
 * The main thread creates the consumer thread and an event queue to it.  
 * Then the main thread sits on an epoll to take external input to 
 * generate events to the event queue.  The consumer thread reads the 
 * event queue and prints the event.  When the consumer receives the 
 * EVT_DONE event, it exits.
 *
 * External input can come from stdin (the user typing symbolic events)
 * or from a script file.  See evt_producer and evt_ondemand for the logic.
 *
 * The producer uses epoll with a periodic timer.  When it expires, this 
 * generates an EVT_TIMER event into the consumer.
 */

#include <stdlib.h>     /* atoi, malloc, strtol, strtoll, strtoul */
#include <stdbool.h>    /* bool type and true, false values */
#include <inttypes.h>   /* include stdint.h, PRI macros, integer conversions */
#include <ctype.h>      /* isalnum */
#include <unistd.h>     /* common typdefs, e.g. ssize_t, includes getopt.h */
#include <errno.h>      /* perror, see /usr/include/asm-generic for E-codes */
#include <time.h>       /* clock_gettime, struct timespec, timer_create */
#include <stdio.h>      /* char I/O */
#include <signal.h>     /* sigaction */
#include <string.h>     /* strlen, strsignal,, memset */
#include <libnl3/netlink/list.h> /* kernel-ish linked list */
#include <pthread.h>    /* posix threads */
#include <sys/epoll.h>  /* epoll_ctl */

/**
 * die - terminate task with a descriptive error message
 * @msg: null-terminated string prepended to system error
 *
 * This is a small helper macro to descriptive but suddenly 
 * exit when an unrecoverable situation occurs.
 */
#define die(msg) do { perror(msg); exit(EXIT_FAILURE); } while (0)

/**
 * cleanly exit event loop 
 */
static bool event_loop_done = false;

/**
 * arguments - descriptive string for all commandline arguments
 */
char *arguments = "\n"					\
	" -s scriptfile: read events from file\n"	\
	" -d: set debug_flag\n"				\
	" -h: this help\n";

/**
 * scriptfile - file name for event script to inject events
 * into consumer.
 */
static char scriptfile[64] = "./evtdemo.script";

/**
 * debug_flag - set the internal debug flag to print more info
 */
static int debug_flag = 0;

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
	
	while((opt = getopt(argc, argv, "s:dh")) != -1) {
		switch(opt) {
		case 's':
			strncpy(scriptfile, optarg, sizeof(scriptfile)-1);
			printf("Setting %s to %s\n", scriptfile, optarg);
		
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

/********************** time section **********************/

/**
 * nap - small sleep
 * @num: number of msecs to nap
 *
 * useful for very small delays, causes thread switch
 */
inline static void nap(int num)
{
	struct timespec t = {.tv_sec=0, .tv_nsec=1e6*num}; /* num ms */
	nanosleep(&t, NULL);
}

/**
 * relax - nap for 1ms 
 *
 * This allows running thread to be rescheduled so other threads can run
 */
inline static void relax()
{
	nap(1);
}

/********************************** debug section *******************************/

/* 
 * _dbg_func - dump debug info to stdout
 * @func: calling function
 * @msg: informational message string
 *
 * This will get current clock and create an information string containing
 * calling function, timestamp and information message, then write string to stdout.
 */
inline static void _dbg_func(const char *func, const char *msg)
{
	struct timespec ts;
	char buff[64];
	
	clock_gettime(CLOCK_MONOTONIC, &ts);
	snprintf(buff, sizeof(buff)-1, "%s:%ld.%09ld %s\n", func, ts.tv_sec, ts.tv_nsec, msg);
	write(1, buff, strlen(buff));
}
#define dbg(msg) _dbg_func(__func__, msg);

/*************************************** event and event queue *************************/

/*
 * events - enum containg all events
 */
typedef enum evt_id {
	EVT_BAD = 0,
	EVT_1,
	EVT_2,
	EVT_3,
	EVT_TIMER,
	EVT_IDLE,
	/* EVT_DONE will cause consumer to exit as soon worker
	 * reads it even if there are events after it in the 
	 * input queue.
	 */
	EVT_DONE,
	EVT_TEST,
	EVT_LAST,
} events;

/*
 * evt_name - mapping from evt_id to a text string for debugging
 */
static const char * const evt_name[] = {
	[EVT_BAD] = "Bad Evt",
	[EVT_1] = "Evt 1",
	[EVT_2] = "Evt 2",
	[EVT_3] = "Evt 3",
	[EVT_TIMER] = "Time Tick",
	[EVT_IDLE] = "Idle Ping",
	[EVT_DONE] = "DONE",
	[EVT_TEST] = "really long string used to test evt name",
	[EVT_LAST] = "NULL",
};

/**
 * struct event
 * @list: kernel-style linked list node
 * @event_id: one of the valid events
 */
struct event {
	struct nl_list_head list;
	events event_id;
};

/**
 * evtq_t - the
 * @len: number of items on queue
 * @head: head of queue
 * @mutex: mutex guarding access to the queue
 * @cond: condition set when an event is added to queue
 *
 * This is user-space implementation of the kernel list management function 
 * https://www.kernel.org/doc/html/v5.1/core-api/kernel-api.html#list-management-functions
 * It uses the netlink/list.h macros.
 */
typedef struct {
	int len;
	struct event head;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
} evtq_t;

/**
 * evtq_create - create a queue instance
 *
 * This will malloc an instance of a queue and
 * initialize it.  Notice the NL_INIT_LIST_HEAD macro.
 */
evtq_t* evtq_create()
{
	evtq_t *pq = malloc(sizeof(evtq_t));

	pthread_mutex_init(&pq->mutex, NULL);
	pthread_cond_init(&pq->cond, NULL);	
	pq->len = 0;
	NL_INIT_LIST_HEAD(&pq->head.list);

	return(pq);
}

/**
 * evtq_destroy - remove all queue structurs
 *
 * this will destroy mutex, condition and free queue memory
 */
void evtq_destroy(evtq_t* pq)
{
	if (NULL == pq)
		return;

	pthread_mutex_destroy(&pq->mutex);
	pthread_cond_destroy(&pq->cond);

	free(pq);
}

/**
 * evtq_push - add an event to the tail of the queue
 * @evtq_p - pointer to event queue
 * @id - the event id to add
 * 
 * lock queue
 * create event, add to queue tail
 * signal condition that there is an new event queued
 * unlock queue
 */
void evtq_push(evtq_t *evtq_p, events id)
{
	struct event *ep;

	pthread_mutex_lock(&evtq_p->mutex);
	
	ep = malloc( sizeof(struct event) );
	ep->event_id = id;
	nl_list_add_tail(&ep->list, &evtq_p->head.list);
	evtq_p->len++;

	pthread_cond_signal(&evtq_p->cond);
	pthread_mutex_unlock(&evtq_p->mutex);

	relax();
}

/**
 * evtq_pop - pop an event from head of queue
 * @evtq_p - pointer to event queue
 * @id_p - update this pointer
 *
 * Return: update @id_p with event on queue head
 *
 * lock queue
 * loop while waiting for condition to be set
 *  note: pthread_cond_wait will block waiting on the cond to be set
 * remove event from queue head set the event id
 * free event memory
 * unlock queue
 */ 
void evtq_pop(evtq_t *evtq_p, events* id_p)
{
	struct event *ep;

	/* lock mutex, must be done before cond_wait */
	pthread_mutex_lock(&evtq_p->mutex);

	/* make sure there is something to pop off q */
	while(0 == evtq_p->len) {
		/* this will unlock mutex and then wait on cond */
		/* On return mutex is re-acquired */
		/* if a cond_signal is sent before this is waiting, the signal will be discarded */
		pthread_cond_wait(&evtq_p->cond, &evtq_p->mutex);
	}

	ep = nl_list_first_entry(&evtq_p->head.list, struct event, list);
	nl_list_del(&ep->list);
	evtq_p->len--;
	*id_p = ep->event_id;
	free(ep);		

	pthread_mutex_unlock(&evtq_p->mutex);
	
}

/**
 * evtq_show - dump of events in queue
 * @evtq_p - pointer to event queue
 *
 * This is used for diagnostic debugging, queues empty too fast
 * to use this during runtime.
 */
int evtq_show(evtq_t *evtq_p)
{
	struct event *ep;
	char msg[32];
	int len;

	pthread_mutex_lock(&evtq_p->mutex);

	if (nl_list_empty(&evtq_p->head.list)) {
		dbg("q empty");
	}

	nl_list_for_each_entry(ep, &evtq_p->head.list, list) {
		sprintf(msg, "%d: id=%u", len++, ep->event_id);
		dbg(msg);
	}
	sprintf(msg, "qsize: %d", evtq_p->len);
	dbg(msg);

	pthread_mutex_unlock(&evtq_p->mutex);
}

/* forward definition */
int evt_ondemand(const char c, evtq_t *evtq_p);

/**
 * evt_script - load events from a file to added to event queue
 * @evtq_p - pointer to event queue
 *
 * The input file is reference by the global scriptfile var.
 * 
 * This is just a shell to read the file for symbolic event ids.  The 
 * events are translate from symbolic to events enum in evt_ondemand()
 */
void evt_script(evtq_t *evtq_p)
{
	FILE *fin;
	int len, i;
	char buf[80];
	
	if (NULL == (fin=fopen(scriptfile, "r")))
		die("unknown fname");

	while (NULL != fgets(buf, sizeof(buf), fin)) {

		/* skip comments and empty lines */
		if (buf[0] == '#' || buf[0] == '\n')
			continue;
		
		len = strlen(buf);
		if (debug_flag)
			printf("buf=%s len=%d\n", buf, len);
		
		for (int i=0; i<len; i++)
			if (isalnum(buf[i]))
			    evt_ondemand(buf[i], evtq_p);
	}

	fclose(fin);
}

/**
 * evt_ondemand - translate symbolic event to internal and push to event q
 *
 * @c: the symbolic event id
 * @evtq_p - pointer to event queue
 * 
 * Each event can be symbolically represented as a single char.  This 
 * function maps this to an internal events id (from an enum) and
 * pushes it to the given event queue.
 * 
 * If the event is not recognized the default IDLE event is pushed
 */
int evt_ondemand(const char c, evtq_t *evtq_p)
{
	switch(c) {
	case 'h':
		printf("\tq: quit worker\n");
		printf("\tx: exit producer and worker (gracefully)\n");
		printf("\t1: %s\n", evt_name[EVT_1]);
		printf("\t2: %s\n", evt_name[EVT_2]);
		printf("\ti: %s\n", evt_name[EVT_IDLE]);
		printf("\tt: %s\n", evt_name[EVT_TIMER]);
		printf("\tT: %s\n", evt_name[EVT_TEST]);
		printf("\tr: run event input script from %s\n", scriptfile);
		printf("\tdefault: %s\n", evt_name[EVT_IDLE]);
		break;
	case 'q':
		evtq_push(evtq_p, EVT_DONE);
		break;
	case 'x':
		event_loop_done = true;
		break;
	case '1':
		evtq_push(evtq_p, EVT_1);
		break;
	case '2':
		evtq_push(evtq_p, EVT_2);
		break;
	case 'i':
		evtq_push(evtq_p, EVT_IDLE);
		break;
	case 't':
		evtq_push(evtq_p, EVT_TIMER);
		break;
	case 'T':
		evtq_push(evtq_p, EVT_TEST);
		break;
	case 'r':
		evt_script(evtq_p);
		break;
	default:
		printf("%c: unknown command\n", c);
		break;
	}
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
	char msg[128];
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
 * consumer - archetype event consumer thread
 * @arg: event queue created by controlling thread
 *
 * This is a simple framework:
 * - pop an event when it is available (evtq_pop blocks until then)
 * - print the event
 * - if it is an EVT_DONE event end loop and exit
 */
void *consumer(void *arg)
{
        events evt_id;
	evtq_t *evtq_p = (evtq_t*) arg;
	bool done = false;
	
	char msg[80];

	dbg("enter and wait for events");
		
	while (!done) {
		evtq_pop(evtq_p, &evt_id);
		snprintf(msg, sizeof(msg), "pop evt=%s", evt_name[evt_id]);
		dbg(msg);
		if (EVT_DONE == evt_id)
			done = true;
	}
	
	dbg("exitting...");
}

/**
 * evt_producer - event production code to queue to consumer
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
void evt_producer(evtq_t *evtq_p)
{
	int epoll_fd;                 /* epoll reference */
	struct epoll_event event;     /* struct to add to the epoll list */
	struct epoll_event events[1]; /* struct return from epoll_wait */

	/* create epoll */
	if (-1 == (epoll_fd=epoll_create1(0)))
		die("epoll");

	/* add stdin to epoll for user control */
	event.data.fd = STDIN_FILENO;
	event.events = EPOLLIN;

	/* add event definition to poll list */
	if (-1 == epoll_ctl(epoll_fd, EPOLL_CTL_ADD, event.data.fd, &event))
		die("epoll_ctl adding a newfd");

	/* event loop */
	printf("Enter event, 'h' for help, 'x' to exit\n");
	fflush(stdout);
	while (!event_loop_done) {
		int nfds; /* number of ready file descriptors */

		if (debug_flag)
			dbg("poll_wait");
		
		/* wait for desciptors or timeout 
		 * n < 0: error
		 * n == 0: timeout, timeout in ms, -1 no timeout
		 * n > 0: number of descriptors with pending I/O
		 */
		nfds = epoll_wait(epoll_fd, events, 1, 2000);

		switch(nfds) {
		case -1:
		{
			/* if poll returns due to an signal interrupt then do nothing 
			 * otherwise die
			 */
			if (errno != EINTR)
				die("epoll_wait");
		}
		break;
		case 0:
		{
			evtq_push(evtq_p, EVT_TIMER);
		}
		break;
		case 1:
		{
			int id = 0;
			char buf[2];
			int len;

			/* bad event or corrupted file descriptor */
			if (!(events[id].events&EPOLLIN))
				die("bad incoming event");

			/* line buffered by tty driver so must hit CR to read */
			len=read(events[id].data.fd, buf, sizeof(buf));
			/* replace CR with string termination */
			buf[len] = '\0';
			if (debug_flag)
				printf("\nread %d: %s\n", len, buf);

			evt_ondemand(buf[0], evtq_p);
		}
		break;
		default:
			die("unknown epoll descriptor");
			break;
		} /* switch */
	} /* while */	
}

/**
 * main - a simple driver for an event producer/consumer framework
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

	pthread_t worker_task;
	evtq_t *evtq_p = NULL;

	parsed_args = cmdline_args(argc, argv);

	/* show commandline arguments that are NOT handled
	 * by cmdline_args.
	 */
	for (int i=parsed_args; i<argc; i++) {
		printf("%d: %s\n", i, argv[i]);
	}
	set_sig_handlers();

	/* create queue to pass to consumer thread */
	evtq_p = evtq_create();
	if (0 != pthread_create(&worker_task, NULL, &consumer, (void *)evtq_p))
		die("fsm");
	
	evt_producer(evtq_p);

	/* evt_producer may not have sent the critical EVT_DONE event to consumer
	 * do so now.  If the consumer has exitted this will go into the void.
	 */
	evtq_push(evtq_p, EVT_DONE);
	
	dbg("waiting for joins");
	pthread_join(worker_task, NULL);

	dbg("cleanup");
	evtq_destroy(evtq_p);
	
	dbg ("done");
}
