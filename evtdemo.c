/*
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
 * file.  See evt_producer and evt_ondemand for the logic.
 * When the producer sends an EVT_DONE, all threads will exit their respective  
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
#include <ctype.h>       /* isalnum */
#include <unistd.h>      /* common typdefs, e.g. ssize_t, includes getopt.h */
#include <errno.h>       /* perror, see /usr/include/asm-generic for E-codes */
#include <time.h>        /* clock_gettime, struct timespec, timer_create */
#include <stdio.h>       /* char I/O */
#include <signal.h>      /* sigaction */
#include <string.h>      /* strlen, strsignal,, memset */
#include <libnl3/netlink/list.h> /* kernel-ish linked list */
#include <pthread.h>     /* posix threads */
#include <sys/epoll.h>   /* epoll_ctl */
#include <sys/timerfd.h> /* timerfd_create, timerfd_settime, timerfd_gettime */
#include <sched.h>       /* sched_yield */

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
static char scriptfile[64] = "./evtdemo.script";

/**
 * debug_flag - set the internal debug flag to print more info
 */
static int debug_flag = 0;

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

/********************** time section **********************/

/**
 * nap - small sleep
 * @ms: number of msecs to nap
 *
 * useful for very small delays, causes thread switch
 */
inline static void nap(uint32_t ms)
{
	struct timespec t;

	t.tv_sec = (ms/1000);
	t.tv_nsec = (ms%1000)*1e6;
	nanosleep(&t, NULL);
}

/**
 * relax - calling thread is rescheduled so waiting threads can run
 * 
 * nap(1) performs a similar function.
 */
inline static void relax()
{
	sched_yield();
}

/**
 * create_timer - create and return the timerfd file descriptor
 *
 * Return: the file descriptor to be used in other timer routines
 *
 * The timerfd can be used in an epoll_wait loop to generate a periodic
 * timer function. This should be done only during initialization and
 * set_timer to manage it.
 */
int create_timer()
{
	int fd;
	
	if (-1 == (fd=timerfd_create(CLOCK_MONOTONIC, 0)))
		die("timerfd_create");

	return(fd);
}

/**
 * set_timer - set a timer to a new periodic timeout tick_ms in the future
 *
 * @timerfd: the unique timer fd
 * @tick_ms: a periodic trigger value in milliseconds

 * Return: the tick_ms
 *
 * This is called from the timer thread to set, reset, cancel
 * a timer.  If ms == 0, the timer is cancelled.  If a running 
 * timer is called, the future timeout is reset to this value.
 */
int set_timer(int timerfd, uint64_t tick_ms)
{
	struct itimerspec ts;
	time_t sec;
	long nsec;

	/* convert ms into timerfd argument 
	 * special case for 0, which cancels the timer
	 */
	sec = tick_ms ? (tick_ms/1000) : 0;
	nsec = tick_ms ? (tick_ms%1000)*1e6 : 0;

	ts.it_value.tv_sec = sec;
	ts.it_value.tv_nsec = nsec;
	ts.it_interval.tv_sec = sec;
	ts.it_interval.tv_nsec = nsec;

	if (-1 == timerfd_settime(timerfd, 0, &ts, NULL))
		die("timerfd_settime");

	return tick_ms;
}

/**
 * stop_timer - stop the timer from generating periodic timeouts
 *
 * @timerfd: the unique timer fd
 *
 * Fully stop the timer, use set_timer to start again.
 */
void stop_timer(int timerfd)
{
	struct itimerspec ts = {0, 0, 0, 0};
	if (-1 == timerfd_settime(timerfd, 0, &ts, NULL))
		die("timerfd_settime");
}

/**
 * get_timer - return the current time left before timeout in ms
 *
 * @timerfd: the unique timer fd
 *
 */
uint64_t get_timer(int timerfd)
{
	struct itimerspec ts;
	
	if (-1 == timerfd_gettime(timerfd, &ts))
		die("timer_gettime");

	return (ts.it_value.tv_sec * 1000L + ts.it_value.tv_nsec / 1e6);
}

/********************* events and event queues *************************/

/*
 * fsm_events_t - enum containg all events
 */
typedef enum evt_id {
	EVT_BAD = 0,
	EVT_TIMER,
	EVT_IDLE,
	/* EVT_DONE will cause consumer to exit when it reads
	 * the event even if there are events after it in the 
	 * input queue.
	 */
	EVT_DONE,
	EVT_LAST,
} fsm_events_t;

/*
 * evt_name - mapping from evt_id to a text string for debugging
 */
static const char * const evt_name[] = {
	[EVT_BAD] = "Bad Evt",
	[EVT_TIMER] = "Time Tick",
	[EVT_IDLE] = "Idle, so... peaceful",
	[EVT_DONE] = "DONE, all tasks will exit",
	[EVT_LAST] = "NULL",
};

/**
 * struct fsm_event
 * @list: kernel-style linked list node
 * @event_id: one of the valid events
 */
struct fsm_event {
	struct nl_list_head list;
	fsm_events_t event_id;
};

/**
 * evtq_t - the
 * @len: number of items on queue
 * @head: head of queue
 * @mutex: mutex guarding access to the queue
 * @cond: condition set when an event is added to queue
 *
 * This is user-space implementation of the kernel list management function 
 * https://www.kesrnel.org/doc/html/v5.1/core-api/kernel-api.html#list-management-functions
 * It uses the netlink/list.h macros.
 */
typedef struct {
	int len;
	struct fsm_event head;
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
void evtq_push(evtq_t *evtq_p, fsm_events_t id)
{
	struct fsm_event *ep;

	pthread_mutex_lock(&evtq_p->mutex);
	
	ep = malloc( sizeof(struct fsm_event) );
	ep->event_id = id;
	nl_list_add_tail(&ep->list, &evtq_p->head.list);
	evtq_p->len++;

	pthread_cond_signal(&evtq_p->cond);
	pthread_mutex_unlock(&evtq_p->mutex);

	relax();
}

/**
 * evt_push_all - 
 */
void evtq_push_all(evtq_t *evtq_pp[], fsm_events_t id)
{
	evtq_push(evtq_pp[0], id);
	evtq_push(evtq_pp[1], id);
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
void evtq_pop(evtq_t *evtq_p, fsm_events_t* id_p)
{
	struct fsm_event *ep;

	/* lock mutex, must be done before cond_wait */
	pthread_mutex_lock(&evtq_p->mutex);

	/* make sure there is something to pop off q */
	while(0 == evtq_p->len) {
		/* this will unlock mutex and then wait on cond */
		/* On return mutex is re-acquired */
		/* if a cond_signal is sent before this is waiting, the signal will be discarded */
		pthread_cond_wait(&evtq_p->cond, &evtq_p->mutex);
	}

	ep = nl_list_first_entry(&evtq_p->head.list, struct fsm_event, list);
	nl_list_del(&ep->list);
	evtq_p->len--;
	*id_p = ep->event_id;
	free(ep);		

	pthread_mutex_unlock(&evtq_p->mutex);
}

/**
 * evtq_len - 
 * @evtq_p - pointer to event queue
 *
 * Return:
 *   queue len - number of events in queue
 *
 * This is used to check if there are items in queue. 
 * Typically the queue pthread_cond will be set when there are events and evtq_pop
 * will wait on it.  This is used when the thread cannot block on the cond_wait.
 */
uint32_t evtq_len(evtq_t *evtq_p)
{
	int len;

	pthread_mutex_lock(&evtq_p->mutex);
	len = evtq_p->len;
	pthread_mutex_unlock(&evtq_p->mutex);
	return(len);
}

inline static void event_show(fsm_events_t evt_id, const char *msg)
{
	char buff[80];
	snprintf(buff, sizeof(buff), "%s %s", msg, evt_name[evt_id]);
	dbg(buff);
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
	struct fsm_event *ep;
	char msg[80];
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
int evt_ondemand(const char c, evtq_t **evtq_pp);

/**
 * evt_script - load events from a file to added to event queue
 * @evtq_p - pointer to event queue
 *
 * The input file is reference by the global scriptfile var.
 * 
 * This is just a shell to read the file for symbolic event ids.  The 
 * events are translate from symbolic to events enum in evt_ondemand()
 */
void evt_script(evtq_t **evtq_pp)
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
				evt_ondemand(buf[i], evtq_pp);
	}

	fclose(fin);
}

/**
 * evt_ondemand - translate symbolic event to internal and push to event q
 *
 * @c: the symbolic event id
 * @evtq_pp - array of event queue pointers
 * 
 * Each event can be symbolically represented as a single char.  This 
 * function maps this to an internal events id (from an enum) and
 * pushes it to the given event queue.
 */
int evt_ondemand(const char c, evtq_t **evtq_pp)
{
	
	switch(c) {
	case 'h':
		printf("\tq: quit workers\n");
		printf("\tx: exit producer and workers (gracefully)\n");
		printf("\ti: %s\n", evt_name[EVT_IDLE]);
		printf("\tt: %s\n", evt_name[EVT_TIMER]);
		printf("\tr: run event input script from %s\n", scriptfile);
		printf("\ts: sleep 5000ms\n");
		printf("\tdefault: %s\n", evt_name[EVT_IDLE]);
		break;
	case 'q':
		/* just exit event threads */
		evtq_push_all(evtq_pp, EVT_DONE);
		break;
	case 'x':
		/* exit event threads and main */
		evtq_push_all(evtq_pp, EVT_DONE);		
		event_loop_done = true;
		break;
	case 'i':
		evtq_push_all(evtq_pp, EVT_IDLE);
		break;
	case 't':
		evtq_push_all(evtq_pp, EVT_TIMER);
		break;
	case 'r':
		evt_script(evtq_pp);
		break;
	case 's':
		dbg("napping for 5000");		
		nap(5000);
		dbg("after nap");
		break;
	default:
	{
		char msg[80];
		sprintf(msg, "unknown command: '%c'", c);
		dbg(msg);
	}
	break;
	} /* switch */
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
 * evt_timer - pthread generating timer events to consumer
 * @arg: event queue array created by controlling thread
 *
 * - create a periodic timer and set it to 1sec interval
 * - use epoll_wait with a short timeout to wait on timer expiration
 * - if epoll timeouts, check input queue
 * - if periodic timer expires, send EVT_TIMER to consumer
 * - this task receives all events but discards EVT_TIMER
 */
void *evt_timer(void *arg)
{
	evtq_t **evtq_pp = (evtq_t**) arg;
	evtq_t *evtq_me = evtq_pp[1];
	evtq_t *evtq_consumer = evtq_pp[0];
	
	fsm_events_t evt_id;
	bool done = false;

	int fd_epoll;                 /* epoll reference */
	int fd_timer;                 /* timerfd reference */
	
	uint64_t res;
	struct epoll_event event;     /* struct to add to the epoll list */
	struct epoll_event events[1]; /* struct return from epoll_wait */

	/* create epoll */
	if (-1 == (fd_epoll=epoll_create1(0)))
		die("epoll");
	
	/* create an fd_timer and add to epoll list */
	event.data.fd = fd_timer = create_timer();
	event.events = EPOLLIN;
	if (-1 == epoll_ctl(fd_epoll, EPOLL_CTL_ADD, event.data.fd, &event))
		die("epoll_ctl for timerfd");

	set_timer(fd_timer, 1000);

	while (!done) {
		int nfds; /* number of ready file descriptors */

		if (debug_flag)
			dbg("poll_wait");
		
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
			if (evtq_len(evtq_me) > 0)
			{
				evtq_pop(evtq_me, &evt_id);
				event_show(evt_id, "pop evt");
				
				switch(evt_id) {
				case EVT_DONE:
					done = true;
					break;
				case EVT_TIMER: /* discard */
					break;
				case EVT_IDLE:
					nap(1000);
					break;
				default:
					break;
				} /* switch */
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
					dbg("push EVT_TIMER");
					evtq_push(evtq_pp[0], EVT_TIMER);
				}
			}
		}
		break;
		} /* switch */
	} /* while */
	
	dbg("exitting...");	
}		
		
/**
 * evt_consumer - archetype event consumer thread
 * @arg: event queue array created by controlling thread
 *
 * This is a simple framework:
 * - pop an event when it is available (evtq_pop blocks until then)
 * - print the event
 * - if it is an EVT_DONE event end loop and exit
 */
void *evt_consumer(void *arg)
{
	evtq_t **evtq_pp = (evtq_t**) arg;
	evtq_t *evtq_me = evtq_pp[0];	
        fsm_events_t evt_id;
	bool done = false;

	dbg("enter and wait for fsm events");
		
	while (!done)
	{
		evtq_pop(evtq_me, &evt_id);
		event_show(evt_id, "pop evt");
		
		switch(evt_id) {
		case EVT_DONE:
			done = true;
			break;
		case EVT_IDLE:
			nap(100);
			break;
		default:
			break;
		} /* switch */
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
void evt_producer(evtq_t *evtq_pp[])
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

					evt_ondemand(buf[0], evtq_pp);
				}
			}
		}
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

	pthread_t task[2];
	evtq_t *evtq_pp[2];

	parsed_args = cmdline_args(argc, argv);

	/* show commandline arguments that are NOT handled
	 * by cmdline_args.
	 */
	for (int i=parsed_args; i<argc; i++) {
		printf("%d: %s\n", i, argv[i]);
	}
	set_sig_handlers();

	/* create queue and consumer task */
	evtq_pp[0] = evtq_create();
	if (0 != pthread_create(&task[0], NULL, &evt_consumer, (void *)evtq_pp))
		die("consumer task");

	/* create queue and timer task */
	evtq_pp[1] = evtq_create();
	if (0 != pthread_create(&task[1], NULL, &evt_timer, (void *)evtq_pp))
		die("timer task");
	
	non_interactive ? evt_script(evtq_pp) : evt_producer(evtq_pp);

	/* evt_producer may not have sent the critical EVT_DONE event to consumer
	 * do so now.  If the consumer has exitted this will go into the void.
	 */
	evtq_push(evtq_pp[0], EVT_DONE);
	evtq_push(evtq_pp[1], EVT_DONE);
	
	dbg("waiting for joins");
	pthread_join(task[0], NULL);
	pthread_join(task[1], NULL);	

	dbg("cleanup");
	evtq_destroy(evtq_pp[0]);
	evtq_destroy(evtq_pp[1]);	

	dbg("exitting...");
}

