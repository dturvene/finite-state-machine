<!--
pandoc prd.stoplight-crosswalk.md -o /tmp/prd.html
google-chrome /tmp/prd.html
-->

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

1. the source state from which this transition is valid
2. a single event causing the transition to a new state
3. a pointer to a function to perform when leaving the current state (exit action)
4. the new state to transition to
5. a pointer to a function to perform entering the new state (entry action)
6. A descriptive text string about the transition.

Transitions are only executed when both the current state matches the from_state 
AND the incoming event matches the transition event. This ensures proper state 
machine behavior and prevents invalid transitions.

The FSM will automatically print a timestamp, FSM name, and the transition 
descriptive text when any valid transition occurs. The timestamp shows 
hours:minutes:seconds.milliseconds format.

## Timer Service Thread
This will send a TIMER event every 10 seconds to all FSM threads.

## Stoplight FSM Thread
The Stoplight FSM will transition on TIMER and BUTTON events following this state cycle:

**State Transitions:**

- INIT -> RED (on START event)
- RED -> GREEN (on TIMER event) 
- GREEN -> YELLOW (on TIMER or BUTTON event)
- YELLOW -> RED (on TIMER event)

**Inter-FSM Communication:**

- Upon entering the RED state, the FSM immediately sends a WALK event to the Crosswalk FSM
- With 6 seconds remaining in the RED state (4 seconds before timer transition), the stoplight FSM sends a BLINKING event to the crosswalk FSM
- Upon entering the GREEN state, the FSM sends a DONT-WALK event to the Crosswalk FSM

**Button Behavior:**
A BUTTON event received while in GREEN state will cause immediate transition to YELLOW state, bypassing the normal timer cycle. The BUTTON event is ignored in all other states.

## Crosswalk FSM Thread
The Crosswalk FSM will transition on WALK, BLINKING and DONT-WALK events following this state cycle:

**State Transitions:**

- INIT -> DONT-WALK (on START event)
- DONT-WALK -> WALK (on WALK event)
- WALK -> BLINKING (on BLINKING event) 
- BLINKING -> DONT-WALK (on DONT-WALK event)

The crosswalk FSM is fully controlled by events sent from the stoplight FSM, ensuring proper synchronization between traffic light and pedestrian crossing signals.

## Events
The system uses a single set of events shared between FSMs:

**Internal Events:**

- START: Initialize FSMs to their starting states
- TIMER: Periodic event generated every 10 seconds
- WALK: Sent from stoplight to crosswalk FSM  
- BLINKING: Sent from stoplight to crosswalk FSM
- DONT-WALK: Sent from stoplight to crosswalk FSM
- EXIT: Terminate all threads and exit program

**User Input Events:**

- BUTTON: Manual crosswalk button press (only effective in GREEN state)

**Event Processing Rules:**

- Events are only processed if there exists a valid transition from the current state for that event type
- Invalid events for the current state are silently discarded  
- Each FSM maintains its last received event for debugging/display purposes
- Event routing is handled through inter-thread communication channels

**User Interface Commands:**
The following commands are read from stdin (case-insensitive, whitespace separated):

* **S**: (start) Send START event to all FSMs to initialize to operational states
* **B**: (button) Send BUTTON event simulating crosswalk button press  
* **D**: (display) Print current state and last received event for each FSM
* **X**: (exit) Send EXIT event to all threads, terminate program gracefully

All other input characters are ignored.
