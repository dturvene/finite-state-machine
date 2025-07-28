# Overview
This is a product requirement document (PRD) for a code test of a standard
stoplight finite state machine (FSM).  It includes a synchronized crosswalk
FSM. A BUTTON event can be generated asynchronously causing the stoplight to
quickly transition from GREEN to YELLOW, otherwise the BUTTON event is discarded.

The user can enter events on stdin that will be sent to the state machines.

# Prompt
```
Read the attached prd.stoplight-crosswalk.md markdown file and create a rust language program from the `Requirements` section.
```

# Requirements
A main routine spawns the three threads and then reads an event token
sequence from stdin.

The finite state machine (FSM) threads are based on the OMG UML Chapter 14
State Machine specification. Each FSM structure is data-driven with the
following fields:
1. FSM name string for display
2. current state
3. last received event
4. A vector of transition structures.

A transition structure contains the following fields:
1. a single event causing the transition to a new state
2. a pointer to a function to perform when leaving the current state
3. the new state to transition to
4. a pointer to a function to perform entering the new state
5. A descriptive text string about the transition.

The entry action for each transition instance will print a timestamp, name of
the current FSM and the transition descriptive text. The timestamp will show
seconds and milliseconds.

## Timer Service Thread
This will send a TIMER event every 10 seconds to all FSM threads.

## Stoplight FSM Thread
The Stoplight FSM will transition on TIMER and BUTTON events.

Upon entering the RED state, the FSM will send a WALK event to the Crosswalk
FSM.

With 4 seconds remaining in the RED state, the stoplight FSM will send a
BLINKING event to the crosswalk FSM.

Upon entering the GREEN state, the FSM will send a DONT-WALK event to the
Crosswalk FSM.

A BUTTON event will cause the FSM to transition from GREEN to YELLOW, wait 4
seconds and then transition to the RED state.

## Crosswalk FSM Thread
The Crosswalk FSM will transition on WALK, BLINKING and DONT-WALK events.

## Events
There is a single set of events.

If the event type is valid in the current state of the FSM, it will cause a
transition to a new state.  If the event is not valid for the FSM in its
current state then it will be silently discarded. The FSM will loop for the
next event. The EXIT event causes the threads and program to exit.

The following event tokens are read from stdin separated by white space 
until the 'X' token is entered. All other text will be discarded. 
* S: (start) send a start event to all threads to transition to the initial state,
* B: (button) the cross-walk button is pressed,
* D: (display) print the current state and last event for each FSM,
* X: (exit) send an exit event to all threads, exit the main loop, join the
  threads and exit the program.
