
This project demonstrates how to create, manage and drive a finite state
machine (FSM).

The code is developed in C and, where possible, uses Linux kernel and
POSIX-based APIs:

* linked list using the `libnl3/netlink/list.h` macros:  This is a user-space
  replica of the kernel `list.h` macros.
* [POSIX threads](https://pubs.opengroup.org/onlinepubs/9699919799/xrat/V4_xsh_chap02.html#tag_22_02_09)
* `pthread_cond`, `pthread_mutex`: these are comparable to the kthread cond and
  mutex calls. 
* [Linux Kernel  Doc]
  (https://www.kernel.org/doc/html/v5.1/doc-guide/kernel-doc.html) comments

# evtdemo.c


The first program, `evtdemo.c` is an event delivery framework.

