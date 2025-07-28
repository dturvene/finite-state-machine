#![allow(unused_variables)]

use std::sync::mpsc::{self, Receiver, Sender};
use std::thread;
use std::time::{Duration, SystemTime, UNIX_EPOCH};
use std::io::{self, BufRead};
// use std::collections::HashMap;

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
    // Stoplight states
    StoplightRed,
    StoplightYellow,
    StoplightGreen,
    
    // Crosswalk states
    CrosswalkDontWalk,
    CrosswalkWalk,
    CrosswalkBlinking,
    
    // Initial state
    Initial,
}

type ActionFn = fn(&str);

#[derive(Clone)]
struct Transition {
    event: Event,
    exit_action: Option<ActionFn>,
    new_state: State,
    entry_action: Option<ActionFn>,
    description: String,
}

struct FSM {
    name: String,
    current_state: State,
    last_event: Option<Event>,
    transitions: Vec<Transition>,
    #[allow(dead_code)]
    event_sender: Option<Sender<(String, Event)>>,
}

impl FSM {
    fn new(name: String, event_sender: Option<Sender<(String, Event)>>) -> Self {
        FSM {
            name,
            current_state: State::Initial,
            last_event: None,
            transitions: Vec::new(),
            event_sender,
        }
    }

    fn add_transition(&mut self, from_state: State, event: Event, exit_action: Option<ActionFn>, 
                     new_state: State, entry_action: Option<ActionFn>, description: String) {
        // Only add transition if it's from the correct state
        if self.current_state == State::Initial || from_state == self.current_state {
            self.transitions.push(Transition {
                event,
                exit_action,
                new_state,
                entry_action,
                description,
            });
        }
    }

    fn process_event(&mut self, event: Event) -> bool {
        self.last_event = Some(event.clone());
        
        if event == Event::Exit {
            return false;
        }

        // Find matching transition
        let transition = self.transitions.iter()
            .find(|t| t.event == event)
            .cloned();

        if let Some(trans) = transition {
            // Execute exit action if present
            if let Some(exit_fn) = trans.exit_action {
                exit_fn(&self.name);
            }

            // Change state
            self.current_state = trans.new_state;

            // Execute entry action and print transition info
            if let Some(entry_fn) = trans.entry_action {
                entry_fn(&self.name);
            }
            print_transition(&self.name, &trans.description);

            // Update transitions for new state
            self.update_transitions_for_state();
        }
        
        true
    }

    fn update_transitions_for_state(&mut self) {
        self.transitions.clear();
        
        match self.current_state {
            State::StoplightRed => {
                self.add_transition(State::StoplightRed, Event::Timer, None, State::StoplightGreen, 
                    Some(send_dont_walk_action), "RED to GREEN on TIMER".to_string());
            },
            State::StoplightYellow => {
                self.add_transition(State::StoplightYellow, Event::Timer, None, State::StoplightRed, 
                    Some(send_walk_action), "YELLOW to RED on TIMER".to_string());
            },
            State::StoplightGreen => {
                self.add_transition(State::StoplightGreen, Event::Timer, None, State::StoplightYellow, 
                    None, "GREEN to YELLOW on TIMER".to_string());
                self.add_transition(State::StoplightGreen, Event::Button, None, State::StoplightYellow, 
                    None, "GREEN to YELLOW on BUTTON".to_string());
            },
            State::CrosswalkDontWalk => {
                self.add_transition(State::CrosswalkDontWalk, Event::Walk, None, State::CrosswalkWalk, 
                    None, "DONT-WALK to WALK on WALK".to_string());
            },
            State::CrosswalkWalk => {
                self.add_transition(State::CrosswalkWalk, Event::Blinking, None, State::CrosswalkBlinking, 
                    None, "WALK to BLINKING on BLINKING".to_string());
                self.add_transition(State::CrosswalkWalk, Event::DontWalk, None, State::CrosswalkDontWalk, 
                    None, "WALK to DONT-WALK on DONT-WALK".to_string());
            },
            State::CrosswalkBlinking => {
                self.add_transition(State::CrosswalkBlinking, Event::DontWalk, None, State::CrosswalkDontWalk, 
                    None, "BLINKING to DONT-WALK on DONT-WALK".to_string());
            },
            State::Initial => {
                if self.name == "Stoplight" {
                    self.add_transition(State::Initial, Event::Start, None, State::StoplightRed, 
                        Some(send_walk_action), "Initial to RED on START".to_string());
                } else if self.name == "Crosswalk" {
                    self.add_transition(State::Initial, Event::Start, None, State::CrosswalkDontWalk, 
                        None, "Initial to DONT-WALK on START".to_string());
                }
            },
        }
    }

    fn display_state(&self) {
        println!("{}: State={:?}, Last Event={:?}", self.name, self.current_state, self.last_event);
    }
}

fn print_transition(fsm_name: &str, description: &str) {
    let now = SystemTime::now().duration_since(UNIX_EPOCH).unwrap();
    let seconds = now.as_secs();
    let millis = now.subsec_millis();
    println!("[{}.{:03}] {}: {}", seconds, millis, fsm_name, description);
}

fn send_walk_action(fsm_name: &str) {
    // This would send WALK event to crosswalk - implemented in stoplight thread
}

fn send_dont_walk_action(fsm_name: &str) {
    // This would send DONT-WALK event to crosswalk - implemented in stoplight thread
}

fn timer_thread(stoplight_sender: Sender<(String, Event)>) {
    loop {
        thread::sleep(Duration::from_secs(10));
        if stoplight_sender.send(("Timer".to_string(), Event::Timer)).is_err() {
            break;
        }
    }
}

fn stoplight_thread(receiver: Receiver<(String, Event)>, crosswalk_sender: Sender<(String, Event)>) {
    let mut fsm = FSM::new("Stoplight".to_string(), Some(crosswalk_sender.clone()));
    fsm.update_transitions_for_state();

    loop {
        if let Ok((_, event)) = receiver.recv() {
            if event == Event::Exit {
                break;
            }

            let old_state = fsm.current_state.clone();
            
            if !fsm.process_event(event.clone()) {
                break;
            }

            // Handle state-specific actions
            match fsm.current_state {
                State::StoplightRed if old_state != State::StoplightRed => {
                    // Send WALK immediately when entering RED
                    let _ = crosswalk_sender.send(("Stoplight".to_string(), Event::Walk));
                    
                    // Spawn a thread to send BLINKING after 6 seconds
                    let crosswalk_sender_clone = crosswalk_sender.clone();
                    thread::spawn(move || {
                        thread::sleep(Duration::from_secs(6));
                        let _ = crosswalk_sender_clone.send(("Stoplight".to_string(), Event::Blinking));
                    });
                },
                State::StoplightGreen if old_state != State::StoplightGreen => {
                    let _ = crosswalk_sender.send(("Stoplight".to_string(), Event::DontWalk));
                },
                State::StoplightYellow if old_state == State::StoplightGreen && event == Event::Button => {
                    // Wait 4 seconds then transition to RED
                    thread::sleep(Duration::from_secs(4));
                    fsm.process_event(Event::Timer);
                    
                    // Send WALK immediately when entering RED via button press
                    let _ = crosswalk_sender.send(("Stoplight".to_string(), Event::Walk));
                    
                    // Spawn a thread to send BLINKING after 6 seconds
                    let crosswalk_sender_clone = crosswalk_sender.clone();
                    thread::spawn(move || {
                        thread::sleep(Duration::from_secs(6));
                        let _ = crosswalk_sender_clone.send(("Stoplight".to_string(), Event::Blinking));
                    });
                },
                _ => {}
            }

            if event == Event::Display {
                fsm.display_state();
            }
        }
    }
}

fn crosswalk_thread(receiver: Receiver<(String, Event)>) {
    let mut fsm = FSM::new("Crosswalk".to_string(), None);
    fsm.update_transitions_for_state();

    loop {
        if let Ok((_, event)) = receiver.recv() {
            if !fsm.process_event(event.clone()) {
                break;
            }

            if event == Event::Display {
                fsm.display_state();
            }
        }
    }
}

fn main() {
    // Create channels
    let (stoplight_sender, stoplight_receiver) = mpsc::channel();
    let (crosswalk_sender, crosswalk_receiver) = mpsc::channel();

    // Clone senders for distribution
    let stoplight_sender_clone = stoplight_sender.clone();
    let crosswalk_sender_clone = crosswalk_sender.clone();

    // Spawn threads
    let timer_handle = thread::spawn(move || {
        // timer_thread(stoplight_sender_clone.clone());
	timer_thread(stoplight_sender.clone());
    });

    let stoplight_handle = thread::spawn(move || {
        stoplight_thread(stoplight_receiver, crosswalk_sender_clone);
    });

    let crosswalk_handle = thread::spawn(move || {
        crosswalk_thread(crosswalk_receiver);
    });

    println!("Stoplight Crosswalk FSM System Started");
    println!("Commands: S (start), B (button), D (display), X (exit)");

    // Read events from stdin
    let stdin = io::stdin();
    for line in stdin.lock().lines() {
        if let Ok(input) = line {
            for token in input.split_whitespace() {
                match token.to_uppercase().as_str() {
                    "S" => {
                        let _ = stoplight_sender_clone.send(("Main".to_string(), Event::Start));
                        let _ = crosswalk_sender.send(("Main".to_string(), Event::Start));
                    },
                    "B" => {
                        let _ = stoplight_sender_clone.send(("Main".to_string(), Event::Button));
                    },
                    "D" => {
                        let _ = stoplight_sender_clone.send(("Main".to_string(), Event::Display));
                        let _ = crosswalk_sender.send(("Main".to_string(), Event::Display));
                    },
                    "X" => {
                        let _ = stoplight_sender_clone.send(("Main".to_string(), Event::Exit));
                        let _ = crosswalk_sender.send(("Main".to_string(), Event::Exit));
                        
                        // Wait for threads to finish
                        let _ = timer_handle.join();
                        let _ = stoplight_handle.join();
                        let _ = crosswalk_handle.join();
                        
                        println!("System shutdown complete");
                        return;
                    },
                    _ => {
                        // Discard invalid tokens as specified
                    }
                }
            }
        }
    }
}
