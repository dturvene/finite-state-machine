# Overview
This is a product requirement document (PRD) for a standard stoplight finite
state machine (FSM) code test.  It includes a synchronized crosswalk FSM.  The
stoplight state transitions every 10 seconds.  A BUTTON event can be generated
asynchronously causing the stoplight to transition from GREEN to YELLOW in 3
seconds, otherwise the BUTTON event is discarded.

The user can enter events on stdin that will be sent to the state machines.

# Prompt
```
Read the requirements section in the prd.stoplight-crosswalk.md markdown file and create a rust language program from the Requirements section.
```

# Requirements
A main routine spawns the three threads and then reads an event token
sequence from stdin.

The finite state machine (FSM) threads are based on the OMG UML Chapter 14
State Machine specification. Each state machine is data-driven using a vector
of transition records with the following fields:
1. a single event causing the transition to a new state
2. a pointer to a function to perform when leaving the current state
3. the new state to transition to
4. a pointer to a function to perform entering the new state
5. A text string indicating the current state and most recent event

## Timer Service Thread
A timer service to asynchronously generate timer events.  It will broadcast a
TIMER event every 10 seconds.

## Stoplight FSM Thread
The Stoplight FSM will transition on TIMER and BUTTON events.

In the GREEN state, the BUTTON event will cause a transition from GREEN to 
YELLOW, wait three seconds and then transition to the RED state. 

Upon entering the RED state, the FSM will send a WALK event to the
Crosswalk FSM.

Upon entering the YELLOW state, the FSM will send a BLINKING event to the
Crosswalk FSM.

Upon entering the GREEN state, the FSM will send a DONT-WALK event to the
Crosswalk FSM.

## Crosswalk FSM Thread
The Crosswalk FSM will transition on WALK, BLINKING and DONT-WALK events.

## Events
The events are universal and will be broadcast to all threads.  If the
event type is valid in the current state, it will cause a transition to a new
state.  If the event is not valid for the FSM in its current state then
it will be discarded.

The following event tokens are read from stdin separated by white space 
until the 'X' token is entered. All other text will be discarded. 
* S: (start) send a start event to all threads to transition to the initial state,
* B: (button) the cross-walk button is pressed,
* D: (display) print the current state and last event for each FSM,
* X: (exit) send an exit event to all threads, exit the main loop, join the
  threads and exit the program.
