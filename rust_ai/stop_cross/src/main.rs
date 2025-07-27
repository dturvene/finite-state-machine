
// 250727:crossbeam_channel Receiver cloned but never used. Disable warning for now.
#[allow(unused_imports)]

use crossbeam_channel::{self, Receiver, Sender};
use std::thread;
use std::time::Duration;
use std::io::{self, BufRead};

#[derive(Debug, Clone, PartialEq)]
enum Event {
    Start,
    Timer,
    Button,
    Walk,
    Blinking,
    DontWalk,
    Display,
    Exit,
}

#[derive(Debug, Clone, PartialEq)]
enum State {
    Initial,
    Green,
    Yellow,
    Red,
    RedBlinking,
    Walk,
    DontWalk,
    Blinking,
}

type ActionFn = fn(&[Sender<Event>]);

#[derive(Clone)]
struct Transition {
    from_state: State,
    event: Event,
    exit_action: Option<ActionFn>,
    next_state: State,
    entry_action: Option<ActionFn>,
    description: String,
}

struct FSM {
    name: String,
    current_state: State,
    last_event: Option<Event>,
    transitions: Vec<Transition>,
}

impl FSM {
    fn new(name: String, transitions: Vec<Transition>) -> Self {
        FSM {
            name,
            current_state: State::Initial,
            last_event: None,
            transitions,
        }
    }

    fn process_event(&mut self, event: Event, other_senders: &[Sender<Event>]) -> i32 {
        if event == Event::Exit {
            return -1;  // EXIT event received
        }

        if event == Event::Display {
            self.display_state();
            return 1;   // Special case: Display is always processed
        }

        // Find matching transition
        for transition in &self.transitions {
            if transition.event == event && self.current_state == transition.from_state {
                // Execute exit action
                if let Some(exit_action) = transition.exit_action {
                    exit_action(other_senders);
                }

                // Transition to new state
                self.current_state = transition.next_state.clone();
                self.last_event = Some(event.clone());

                // Execute entry action
                if let Some(entry_action) = transition.entry_action {
                    entry_action(other_senders);
                }

                println!("{}: {} on {:?}", 
                    self.name, transition.description, event);
                return 1;   // Transition occurred
            }
        }

        // Event not valid for current state - discard
        0   // Event discarded
    }

    fn display_state(&self) {
        println!("{}: State={:?}, Last Event={:?}", 
            self.name, self.current_state, self.last_event);
    }
}

/*
// Action functions for stoplight FSM entry actions
fn send_walk_event(senders: &[Sender<Event>]) {
    println!("  -> Sending WALK event to Crosswalk FSM");
    // Send to crosswalk FSM (index 2)
    if senders.len() > 2 {
        let _ = senders[2].send(Event::Walk);
    }
}
*/

fn send_blinking_event(senders: &[Sender<Event>]) {
    println!("  -> Sending BLINKING event to Crosswalk FSM");
    // Send to crosswalk FSM (index 2)
    if senders.len() > 2 {
        let _ = senders[2].send(Event::Blinking);
    }
}

fn send_dont_walk_event(senders: &[Sender<Event>]) {
    println!("  -> Sending DONT-WALK event to Crosswalk FSM");
    // Send to crosswalk FSM (index 2)
    if senders.len() > 2 {
        let _ = senders[2].send(Event::DontWalk);
    }
}

// 250727:DST unused variable
fn button_delay_action(_senders: &[Sender<Event>]) {
    println!("  -> Button pressed, waiting 3 seconds before continuing");
    thread::sleep(Duration::from_secs(3));
}

fn red_entry_action(senders: &[Sender<Event>]) {
    println!("  -> Entering RED state, sending WALK event");
    // Send WALK event immediately upon entering RED
    if senders.len() > 2 {
        let _ = senders[2].send(Event::Walk);
    }
    
    // Spawn a thread to handle the 6-second delay before blinking warning
    let red_senders = senders.to_vec();
    thread::spawn(move || {
        thread::sleep(Duration::from_secs(6)); // Wait 6 seconds
        println!("  -> RED state: 4 seconds remaining, sending internal timer");
        // Send timer event to trigger RED -> RedBlinking transition
        if red_senders.len() > 1 {
            let _ = red_senders[1].send(Event::Timer);
        }
    });
}

fn send_timer_to_fsms(senders: &[Sender<Event>]) {
    println!("Timer service: Sending TIMER event to FSMs");
    // Send to stoplight FSM (index 1) and crosswalk FSM (index 2)
    for i in 1..senders.len() {
        let _ = senders[i].send(Event::Timer);
    }
}

fn main() {
    // Create dedicated channels for each thread
    // Index 0: Timer service
    // Index 1: Stoplight FSM  
    // Index 2: Crosswalk FSM
    let (timer_tx, timer_rx) = crossbeam_channel::unbounded::<Event>();
    let (stoplight_tx, stoplight_rx) = crossbeam_channel::unbounded::<Event>();
    let (crosswalk_tx, crosswalk_rx) = crossbeam_channel::unbounded::<Event>();
    
    // Create array of all senders for passing to threads
    let all_senders = vec![timer_tx.clone(), stoplight_tx.clone(), crosswalk_tx.clone()];
    
    // Timer service thread - sends TIMER events to FSMs every 10 seconds
    let timer_senders = all_senders.clone();
    let timer_handle = thread::spawn(move || {
        println!("Timer service started");
        
        // Spawn timer thread that sends TIMER events every 10 seconds
        let timer_senders_inner = timer_senders.clone();
	// 250727:DST unused variable
        let _timer_thread = thread::spawn(move || {
            loop {
                thread::sleep(Duration::from_secs(10));
                send_timer_to_fsms(&timer_senders_inner);
            }
        });

        // Timer service listens for exit event
        loop {
            match timer_rx.recv() {
                Ok(Event::Exit) => {
                    println!("Timer service: Received EXIT event");
                    break;
                }
                Ok(_) => continue,
                Err(_) => break,
            }
        }
        
        println!("Timer service terminated");
    });

    // Stoplight FSM thread - processes TIMER and BUTTON events
    let stoplight_senders = all_senders.clone();
    let stoplight_handle = thread::spawn(move || {
        println!("Stoplight FSM started");
        
        let mut stoplight_fsm = FSM::new(
            "Stoplight".to_string(),
            vec![
                Transition {
                    from_state: State::Initial,
                    event: Event::Start,
                    exit_action: None,
                    next_state: State::Green,
                    entry_action: Some(send_dont_walk_event),
                    description: "Initial -> Green".to_string(),
                },
                Transition {
                    from_state: State::Green,
                    event: Event::Timer,
                    exit_action: None,
                    next_state: State::Yellow,
                    entry_action: Some(send_blinking_event),
                    description: "Green -> Yellow (Timer)".to_string(),
                },
                Transition {
                    from_state: State::Yellow,
                    event: Event::Timer,
                    exit_action: None,
                    next_state: State::Red,
                    entry_action: Some(red_entry_action),
                    description: "Yellow -> Red (Timer)".to_string(),
                },
                Transition {
                    from_state: State::Red,
                    event: Event::Timer,
                    exit_action: None,
                    next_state: State::RedBlinking,
                    entry_action: Some(send_blinking_event),
                    description: "Red -> RedBlinking (6 seconds elapsed, 4 seconds left)".to_string(),
                },
                Transition {
                    from_state: State::RedBlinking,
                    event: Event::Timer,
                    exit_action: None,
                    next_state: State::Green,
                    entry_action: Some(send_dont_walk_event),
                    description: "RedBlinking -> Green (Timer)".to_string(),
                },
                Transition {
                    from_state: State::Green,
                    event: Event::Button,
                    exit_action: Some(button_delay_action),
                    next_state: State::Yellow,
                    entry_action: Some(send_blinking_event),
                    description: "Green -> Yellow (Button)".to_string(),
                },
            ],
        );

        loop {
            match stoplight_rx.recv() {
                Ok(event) => {
                    let result = stoplight_fsm.process_event(event, &stoplight_senders);
                    if result == -1 {
                        println!("Stoplight FSM: Received EXIT event");
                        break;
                    }
                    // result == 1: transition occurred, result == 0: event discarded
                }
                Err(_) => break,
            }
        }
        println!("Stoplight FSM terminated");
    });

    // Crosswalk FSM thread - processes WALK, BLINKING, DONT-WALK events
    let crosswalk_senders = all_senders.clone();
    let crosswalk_handle = thread::spawn(move || {
        println!("Crosswalk FSM started");
        
        let mut crosswalk_fsm = FSM::new(
            "Crosswalk".to_string(),
            vec![
                Transition {
                    from_state: State::Initial,
                    event: Event::Start,
                    exit_action: None,
                    next_state: State::DontWalk,
                    entry_action: None,
                    description: "Initial -> DontWalk".to_string(),
                },
                Transition {
                    from_state: State::DontWalk,
                    event: Event::Walk,
                    exit_action: None,
                    next_state: State::Walk,
                    entry_action: None,
                    description: "DontWalk -> Walk".to_string(),
                },
                Transition {
                    from_state: State::Walk,
                    event: Event::Blinking,
                    exit_action: None,
                    next_state: State::Blinking,
                    entry_action: None,
                    description: "Walk -> Blinking".to_string(),
                },
                Transition {
                    from_state: State::Blinking,
                    event: Event::DontWalk,
                    exit_action: None,
                    next_state: State::DontWalk,
                    entry_action: None,
                    description: "Blinking -> DontWalk".to_string(),
                },
                Transition {
                    from_state: State::Walk,
                    event: Event::DontWalk,
                    exit_action: None,
                    next_state: State::DontWalk,
                    entry_action: None,
                    description: "Walk -> DontWalk".to_string(),
                },
            ],
        );

        loop {
            match crosswalk_rx.recv() {
                Ok(event) => {
                    let result = crosswalk_fsm.process_event(event, &crosswalk_senders);
                    if result == -1 {
                        println!("Crosswalk FSM: Received EXIT event");
                        break;
                    }
                    // result == 1: transition occurred, result == 0: event discarded
                }
                Err(_) => break,
            }
        }
        println!("Crosswalk FSM terminated");
    });

    // Main loop - read events from stdin and send to appropriate threads
    println!("Stoplight/Crosswalk FSM System");
    println!("Commands: S (start), B (button), D (display), X (exit)");
    println!("Timer events will be generated automatically every 10 seconds after start");
    
    let stdin = io::stdin();
    for line in stdin.lock().lines() {
        let line = line.unwrap();
        for token in line.split_whitespace() {
            match token.to_uppercase().as_str() {
                "S" => {
                    println!("Main: Sending START event to all FSMs");
                    let _ = stoplight_tx.send(Event::Start);
                    let _ = crosswalk_tx.send(Event::Start);
                }
                "B" => {
                    println!("Main: Sending BUTTON event to Stoplight FSM");
                    let _ = stoplight_tx.send(Event::Button);
                }
                "D" => {
                    println!("Main: Sending DISPLAY event to all FSMs");
                    let _ = stoplight_tx.send(Event::Display);
                    let _ = crosswalk_tx.send(Event::Display);
                    thread::sleep(Duration::from_millis(200)); // Allow display to complete
                }
                "X" => {
                    println!("Main: Sending EXIT event to all threads");
                    let _ = timer_tx.send(Event::Exit);
                    let _ = stoplight_tx.send(Event::Exit);
                    let _ = crosswalk_tx.send(Event::Exit);
                    break;
                }
                _ => {
                    // Discard invalid tokens as specified
                }
            }
        }
        
        if line.to_uppercase().contains("X") {
            break;
        }
    }

    // Wait for threads to finish
    let _ = timer_handle.join();
    let _ = stoplight_handle.join();
    let _ = crosswalk_handle.join();
    
    println!("Program terminated.");
}
