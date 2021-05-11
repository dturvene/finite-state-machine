<!--
github push:
sign_and_send_pubkey: signing failed: agent refused operation
git@github.com: Permission denied (publickey).
fatal: Could not read from remote repository.
See ssh.sh:ssh_agent
-->

Abstract
========
This project demonstrates how to create, manage, drive and regression test a
deterministic finite state machine (DFSM).  The motivation for
developing this code is as follows: 

* Many software-managed elements can be modelled using a theoretical state
  machine.
* Some Linux drivers and network code developed to control these elements have
  a software implementation of the DFSM.
* Many of the FSM implementations are very difficult to comprehend.  As a
  simple example of this: study `$K/include/net/tcp_states.h` and calls to
  `tcp_set_state` (`$K/net/ipv4/tcp.c`). Try to match the FSM defined in 
  [RFC-793:TCP](https://tools.ietf.org/html/rfc793) with the code.
* TCP has a fairly simple FSM. Some Telecom chipsets have much
  more complex FSMs or multiple FSMs cooperating to fulfill a mission. These
  FSMs can be fragile and hard to debug.
  
There are two substantial efforts under this project.

1. The first is to develop a simple, reliable event delivery framework to/from
   a set of POSIX "worker" threads: [evtdemo](#evtdemo).
   
2. The second uses the event delivery framework to build the 
   [FSM Example](#fsm-example): [fsmdemo](#fsmdemo).
     
FSM Overview
============
A DFSM is essentially a set of states, each accepting a subset of all FSM
events.  Each event causes a reaction particular to that state.  An event not
accepted by the current state will either be discarded or cause an error.  It
is possible that an error will cause the FSM to generate an error event to
another FSM alerting it to a possible FSM bug.

The FSM model is based on [OMG UML 2.5.1][] specification.  This is a
comprehesive (800 page!) description of all aspects of the UML but I will focus
Chapter 14 (State Machines) and even then not implement much described
behavior.

The basic requirements for the FSMs implemented in this project are:
* simple to understand,
* deterministic,
* minimal number of events generated/consumed.

I decided to send all events to all FSMs to simplify the event delivery
framework. Thus a FSM that generates an event will also receive it. If it is
in state to accept the event then the transition logic will be run, otherwise
the event will be discarded.  At the `DBG_DEEP` verbosity (`-d 0x20`) a 
`NO match` debug message will be geneate by the fsm logic.

Each Transition (UML 14.2.3.8) is a struct of:
* current state
* event (`E_`)
* transition guard constaint function
* next state

As described in [OMG UML v2.5.1][] paragraph 14.2.3.8.3, a transition
may have a guard condition. This boolean condition will allow the transition if
`true` and deny it if `false`.  In otherwords, if the guard condition returns
`false` the transition to the next state will not be completed and the event
will be discarded.

Each State (UML 14.2.3.4) is a struct of:
* char name
* entry_action function (UML 14.2.3.4.5)
* exit_action function (UML 14.2.3.4.6)

Finally, an FSM definition is simply a static array of Transition
instances. 

The code in `fsm.[ch]` is the generic code for an FSM, including a
call to `fsm_run` to drive the FSM given an input event.

The code in `fsm_defs.h` contains the definition for the stoplight and
crosswalk FSMs along with the (simple) `entry_action`, `exit_action` and
`constraint` functions.  It also contains the specifics for the timers used by 
these two FSMs. 

Glossary and Definitions
========================
* [C language](https://en.wikipedia.org/wiki/C_(programming_language))
* [CLI](https://en.wikipedia.org/wiki/Command-line_interface): Command Line Interface
* [DFSM](https://en.wikipedia.org/wiki/Deterministic_finite_automaton): 
  Deterministic Finite State Machine
* `$K`: the root of the Linux kernel source tree

FSM Example
===========
The project uses a set of simple, interworking FSMs to illustrate the
concepts.  In this example, there are four threads:

1. `MGMT` is the main process of the event framework to start/stop the FSMs and
   send events to them. It has no states.  It can generate the following events
   either from the keyboard or a script file.
   
2. `FSM1` is the thread for the traffic stoplight.

3. `FSM2` is the thread for the crosswalk.

4. `TSRV` is the timer service.

The `MGMT` thread has no states but can generate on-demand events from a user
interface or script file 
([Unit and Regression Testing](#unit-and-regression-testing)).

`TSRV` is a service used by the other threads to create/manage timers and timer
events.  A worker creates and sets a timer with the TSRV thread using the
`fsmtimer` API.  The `fsmtimer` API has an internal mutex to protect against
race conditions.  When a timer expires, the `TSRV` delivers the corresponding
event to all worker threads.

FSM1 (stoplight) state diagram
------------------------------
![FSM1](fsm_stoplight.png)

FSM2 (crosswalk) state diagram
------------------------------
![FSM2](fsm_crosswalk.png)

Software Overview
=================

FSM Definition
--------------
An FSM is defined as a set of Transitions from one state to the next when an
Event is injected into the FSM driver function.

The following data structures are used to programmatically define an
FSM. The structures are organized from most elemental to 
  
* Event : A simple enum of event symbols, each using an `E_` prefix.
  See `evt_q.h` for documentation.

* State: A structure defining a specific state, with entry and exit actions.
  See `fsm.h` for documentation.

* Transition: A structure defining a current state, next state and the event
  that causes the transition. See `fsm.h` for documentation.

* FSM: A simple array of transition instances. See `fsm_defs.h` for
  documentation.
  
The FSM mechanisms use a small subset of features in the State Machines chapter
of [OMG UML v2.5.1](https://www.omg.org/spec/UML/2.5.1/). However, all the
features of each FSM are defined in the UML doc.

Software APIs
-------------
Per the [Abstract](#abstract), I am focussing on the Linux kernel and device
drivers.  The code is developed in C and, where possible, mimics the 
[Linux kernel API](https://www.kernel.org/doc/html/v5.11/core-api/kernel-api.html)
Where a suitable kernel API was not possible, the software uses
[POSIX](https://pubs.opengroup.org/onlinepubs/9699919799/)
APIs.

The APIs used in this project include the following.

* The linked list use the `libnl3/netlink/list.h` macros, which are a
  replica of the macros in
  [kernel list management][]
  and implemented in `$K/include/linux/list.h`.
* `pthread_`, `pthread_cond_` calls as defined in
  [POSIX threads][]
  are roughly comparable to the kernel `kthread_` and `wait_event_` APIs
  described in 
  [Kernel Basics](https://www.kernel.org/doc/html/v5.1/driver-api/basics.html)
* `pthread_mutex_` calls are comparable to the mutex APIs documented in
  [locking](https://www.kernel.org/doc/html/v5.1/kernel-hacking/locking.html)
  
Unit and Regression Testing
---------------------------
The FSMs are entirely event driven, with event generation from timer expiry or
the CLI.  The CLI accepts a simple set of string commands from the user for
interaction with the FSMs.  The commands are one of:

1. FSM event generation
2. timer control
3. FSM status
4. Pause the CLI thread while the FSMs run

This is an effective mechanism to unit test the FSMs.

The FSMs can be regression tested by combining CLI commands into a script.  As
the FSMs cycle through state transitions, the script either pauses or retrieves
FSM status.  The script is passed as an argument to [fsmdemo][] and visually
checked.  Ideally I would wrap the FSM scripts in a python test script to
verify that the FSMs are in the correct state and the timers are set to the
correct expiry values.

Here is the `button.script` to test the button press support:

```
# test script to test button press
# ./fsmdemo -n -s button.script -t 100
# Test the button event

# send workers go event to run and nap
# light timer=t_norm(10*tick), state=S:GREEN
g n1 s

# button press, nap1, status
# light timer=t_but(1*tick), state=S:GREEN_BUT
b n1 s

# wait for going out of GREEN_BUT
# light timer=t_fast(3*tick), state=S:YELLOW
n3 s

# nap3, status
# light timer=t_norm(10*tick), state=S:RED,S:WALK
n3 s

# button, nap1, status
# light timer=t_norm(10*tick), state=S:RED,S:WALK
b n1 s

# nap5, nap5, status
# light timer=t_norm(10*tick), state=S:GREEN,S:DONT_WALK
n5 n5 s

# exit all threads and join
x
# script eof
```

Alternative API Implementations
-------------------------------
These are some of the kernel mechanisms I investigated as alternatives to the
classic `mutex/cond_wait` and linked list APIs from above.  The [FIFO](#fifo) and
[reader-writer lock](#reader-writer-lock) are worthy of more investigation to
replace the current `mutex/cond_wait` implementation.

Events
------
I looked at the [libevent](http://libevent.org/) library for event handling,
which uses a registration/callback API.

Currently the event delivery code uses a simple queue based on a user-space
implementation of [kernel list management][] and `pthread_cond_` calls from
[POSIX threads][].  This is similar to some kernel event delivery mechanisms,
search on `enqueue` and `dequeue` for examples.  This mechanism is simple and
meets all requirements so I don't see changing it.

FIFO
----
I looked at using a
[Kernel FIFO Buffer](https://www.kernel.org/doc/html/v5.11/core-api/kernel-api.html#fifo-buffer)
implementation for the event interface to the FSM, probably with 
[mkfifo](https://man7.org/linux/man-pages/man3/mkfifo.3.html) or
[pipe](https://man7.org/linux/man-pages/man2/pipe.2.html).  The fifo and pipe
have locking internal to the API.

I chose to use kernel lists.  Either would suffice.

Reader-writer lock
------------------
An alternative to a mutex is the reader-writer lock, which would be good for this
one-way queue between threads.  The kernel API is
`$K/kernel/locking/qrwlock.c`.  It uses
[spinlock](https://en.wikipedia.org/wiki/Spinlock) and
[atomic operation](https://wiki.osdev.org/Atomic_operation)
to protect the queue.

The comparable pthread implementation is `pthread_rwlock_*`.  This is worth
explore for the next release.

<!--
https://docs.oracle.com/cd/E19455-01/806-5257/6je9h032u/index.html
https://docs.oracle.com/cd/E26502_01/html/E35303/sync-124.html
https://en.wikipedia.org/wiki/Readers%E2%80%93writer_lock
-->

Kernel workqueue
----------------
The 
[Kernel workqueue](https://www.kernel.org/doc/html/v5.11/core-api/workqueue.html) 
mechanism is attractive but too complex for the purpose of this project.

Kernel RCU
----------
The 
[Kernel RCU](https://www.kernel.org/doc/html/v5.11/RCU/index.html) mechanism is
another powerful API.  Essentially it protects concurrent access by
doing sequential Read, Copy, Update steps.  The idea is the writing task copies
the data structure while other tasks are reading it, modifies the data
structure copy and block new readers from accessing the original.  When all
current readers are done with the data structure, the writer replaces the
original structure with the copy then allowing reader task access to the
updated structure. 

This is more efficient than a lock, which blocks all access to the data
structure while it is being updated, because the RCU model allows readers
access the data structure while the copy is being modified.

The most notable user-space RCU library is [URCU](http://liburcu.org/).

As with the workqueue, RCU is too complex for the purpose of this project.

<!--
https://www.kernel.org/doc/html/v5.11/kernel-hacking/locking.html#avoiding-locks-read-copy-update
https://www.kernel.org/doc/html/v5.11/RCU/index.html
-->

Project Documentation
=====================
This **README** is the primary project documentation.

The source code is heavily documented using the 
[Kernel Doc](https://www.kernel.org/doc/html/v5.1/doc-guide/kernel-doc.html)
syntax.

evtdemo
=======
The first program, `evtdemo.c` is an event delivery framework based on a
producer/consumer architecture.  MGMT sends `fsm_events` to two worker threads.
Each worker thread will call the TSRV API to create and start one or more
timers managed by the TSRV thread.

The `TSRV` thread uses the Linux
[epoll](https://man7.org/linux/man-pages/man7/epoll.7.html) and
[timerfd](https://man7.org/linux/man-pages/man2/timerfd_create.2.html)
APIs to implement timers. It can support a maximum of four concurrent timers.

See the inline documentation for more information.

fsmdemo
=======
This program implements the [FSM Example](#fsm-example) using the `evtdemo.c`
framework.

<!--
References
-->
[kernel list management]: https://www.kernel.org/doc/html/v5.11/core-api/kernel-api.html#list-management-functions
[POSIX threads]: https://pubs.opengroup.org/onlinepubs/9699919799/xrat/V4_xsh_chap02.html#tag_22_02_09
[OMG UML v2.5.1]: https://www.omg.org/spec/UML/2.5.1/
