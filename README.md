This project demonstrates how to create, manage and drive a finite state machine (FSM) developed in C and using Linux kernel(ish) APIs.  The code is run in user-space but tracks well with the Linux Kernel and consistent embedded OS APIs. APIs include:

* linked list using the `libnl3/netlink/list.h` macros:  This is a user-space replica of the kernel `list.h` macros.
* pthread_cond, pthread_mutex: these track to the kthread cond and mutex calls.

The first program, `evtd.c` is an event delivery framework.

