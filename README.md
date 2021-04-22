
Abstract
========
This project demonstrates how to create, manage, drive and regression test a
finite state machine (FSM).  The motivation for developing this code is: 

1. Many software-managed elements can be modelled using a theoretical state
   machine.
2. Some Linux drivers and network code developed to control these elements have
   a software implementation of the FSM.
3. Many of the FSM implementations are very difficult to comprehend.  As a
   simple example of this: study `$K/include/net/tcp_states.h` and calls to
   `tcp_set_state` (`$K/net/ipv4/tcp.c`). The FSM is hard to track with
   [RFC-793:TCP](https://tools.ietf.org/html/rfc793).
4. TCP has a fairly simple FSM. Some complex chipsets used in Telecom have much
   more complex FSMs or multiple FSMs cooperating to fulfill a mission.  These
   FSMs can be fragile and hard to debug.
   
Because we are focussing on the Linux kernel the code is developed in C 
and, where possible, mimics the 
[Linux kernel API](https://www.kernel.org/doc/html/v5.11/core-api/kernel-api.html)
Where not possible, it uses
[POSIX](https://pubs.opengroup.org/onlinepubs/9699919799/)
based APIs.  The APIs include:

* The linked list use the `libnl3/netlink/list.h` macros, which are a
  replica of the macros in
  [kernel list management](https://www.kernel.org/doc/html/v5.11/core-api/kernel-api.html#list-management-functions)
  and implemented in `$K/include/linux/list.h`.
* [POSIX threads](https://pubs.opengroup.org/onlinepubs/9699919799/xrat/V4_xsh_chap02.html#tag_22_02_09)
* `pthread_`, `pthread_cond_` calls are comparable to the
  kernel `kthread_` and `wait_event_` APIs documented in
  [Kernel Basics](https://www.kernel.org/doc/html/v5.1/driver-api/basics.html),
* `pthread_mutex_` calls are comparable to the mutex APIs documented in
  [locking](https://www.kernel.org/doc/html/v5.1/kernel-hacking/locking.html)
  
Glossary and Definitions
========================
* [Kernel API]: 
* [C language](https://en.wikipedia.org/wiki/C_(programming_language)
* [FSM](https://en.wikipedia.org/wiki/Finite-state_machine): Finite State Machine
* `$K`: the root of the Linux kernel source tree
* [spinlock](https://en.wikipedia.org/wiki/Spinlock)
* [atomic operation](https://wiki.osdev.org/Atomic_operation)
  
Alternative Implementations
===========================

Kernel FIFO
-----------
I looked at using a
[Kernel FIFO Buffer](https://www.kernel.org/doc/html/v5.11/core-api/kernel-api.html#fifo-buffer)
implementation for the event interface to the FSM, probably with 
[mkfifo](https://man7.org/linux/man-pages/man3/mkfifo.3.html) or
[pipe](https://man7.org/linux/man-pages/man2/pipe.2.html).  The fifo and pipe
have locking internal to the API.

I chose to use kernel lists.  Either would suffice.

Kernel reader-writer lock
--------------------------
An alternative to a mutex is the reader-writer lock, which would be ideal for this
one-way queue between threads.  The kernel API is
`$K/kernel/locking/qrwlock.c`.  It uses spinlocks and atomic operations to
protect the queue.

The comparable pthread implementation is `pthread_rwlock_*`.  This is worth
explore for the next release.

Kernel RCU lock
---------------

Kernel workqueue
----------------
[Kernel workqueues](https://www.kernel.org/doc/html/v5.11/core-api/workqueue.html)
are an enticing core tool but too complex for the purpose of this project.

Documentation
-------------
This **README** is the primary project documentation.

The source code is heavily documented using the 
[Kernel Doc](https://www.kernel.org/doc/html/v5.1/doc-guide/kernel-doc.html)
syntax.

evtdemo.c
=========
The first program, `evtdemo.c` is an event delivery framework.

