/*
 * SPDX-License-Identifier: GPL-2.0
 * Copyright (C) 2021 Dahetral Systems
 * Author: David Turvene (dturvene@dahetral.com)
 *
 * command line interface for the FSM project
 */

#include <utils.h>
#include <timer.h>
#include <evtq.h>
#include <workers.h>


/* default or set in the program arguments */
extern char scriptfile[];

/* forward ref for evt_script */
int evt_ondemand(const char c);

/**
 * evt_script - load events from a file to added to event queue
 *
 * The input file is reference by the global scriptfile var.
 * 
 * This is just a shell to read the file for symbolic event ids.  The 
 * events are translate from symbolic to events enum in evt_ondemand()
 */
void evt_script(void)
{
	FILE *fin;
	int len, i;
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

		/* for each char in this line of the script file 
		 * check if is an alphanum and call evt_ondemand
		 * to process event char
		 */
		for (int i=0; i<len; i++)
			if (isalnum(buf[i]))
				evt_ondemand(buf[i]);
	}

	fclose(fin);
}

/**
 * evt_ondemand - translate symbolic event to internal and push to all worker
 * event queues.
 *
 * @c: the symbolic event id
 * 
 * Each event can be symbolically represented as a single char.  This 
 * function maps the char to an internal events id (from an enum) and
 * pushes it to all workers.
 */
int evt_ondemand(const char c)
{
	int ret = 0;
	switch(c) {
	case 'h':
		printf("\tx: exit producer and workers (gracefully)\n");
		printf("\tw: show workers and curr state\n");
		printf("\tb: crosswalk button push\n");
		printf("\tg: go %s\n", evt_name[EVT_INIT]);
		printf("\tf: set timer fast\n");
		printf("\ti: %s\n", evt_name[EVT_IDLE]);
		printf("\tt: %s\n", evt_name[EVT_TIMER]);
		printf("\tr: run event input script %s\n", scriptfile);
		printf("\t1: toggle timer 1\n");
		printf("\t2: toggle timer 2\n");
		printf("\t3: toggle timer 3\n");		
		printf("\tn: nap 5000ms\n");
		printf("\tdefault: %s\n", evt_name[EVT_IDLE]);
		break;
	case 'x':
		/* exit event threads and main */
		workers_evt_broadcast(EVT_DONE);
		ret = 1;
		break;
	case 'w':
		show_workers();
		break;
	case 'g':
		workers_evt_broadcast(EVT_INIT);
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
	case '1':
	case '2':
	case '3':
	{
		uint32_t timerid = (uint32_t)(c - 0x30); /* ascii char to int */
		toggle_timer(timerid);
	}
	break;
	case 'b':
		workers_evt_broadcast(EVT_BUTTON);
		break;
	case 'i':
		workers_evt_broadcast(EVT_IDLE);		
		break;
	case 't':
		workers_evt_broadcast(EVT_TIMER);		
		break;
	case 'r':
		evt_script();
		break;
	case 'n':
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

	return(ret);
}


