use std::sync::mpsc;
use std::thread;
use std::time::{Duration, Instant};
use std::io::{self, Write};

#[derive(Debug, Clone, Copy, PartialEq)]
enum StoplightState {
    Red,
    Yellow,
    Green,
}

#[derive(Debug, Clone, Copy, PartialEq)]
enum CrosswalkState {
    DontWalk,
    Walk,
}

#[derive(Debug, Clone)]
enum Event {
    Timer,
    ButtonPress,
}

#[derive(Debug, Clone)]
struct SystemState {
    stoplight: StoplightState,
    crosswalk: CrosswalkState,
    button_pressed: bool,
    button_timer_start: Option<Instant>,
    yellow_timer_start: Option<Instant>,
}

impl SystemState {
    fn new() -> Self {
        SystemState {
            stoplight: StoplightState::Red,
            crosswalk: CrosswalkState::Walk,
            button_pressed: false,
            button_timer_start: None,
            yellow_timer_start: None,
        }
    }

    fn display(&self) {
        print!("\r");
        match self.stoplight {
            StoplightState::Red => print!("[R] RED    "),
            StoplightState::Yellow => print!("[Y] YELLOW "),
            StoplightState::Green => print!("[G] GREEN  "),
        }
        
        match self.crosswalk {
            CrosswalkState::Walk => print!("| [WALK] WALK     "),
            CrosswalkState::DontWalk => print!("| [STOP] DON'T WALK"),
        }
        
        if self.button_pressed {
            print!(" | BUTTON PRESSED");
        }
        
        io::stdout().flush().unwrap();
    }
}

fn main() {
    println!("Traffic Light System Starting...");
    println!("Press Enter to simulate button press, Ctrl+C to exit");
    println!("----------------------------------------");

    let (event_tx, event_rx) = mpsc::channel::<Event>();
    let (state_tx, state_rx) = mpsc::channel::<SystemState>();
    
    // Clone senders for different threads
    let timer_event_tx = event_tx.clone();
    let button_event_tx = event_tx.clone();

    // Timer thread - generates timer events every 100ms for smooth updates
    thread::spawn(move || {
        loop {
            thread::sleep(Duration::from_millis(100));
            if timer_event_tx.send(Event::Timer).is_err() {
                break;
            }
        }
    });

    // Button input thread
    thread::spawn(move || {
        let stdin = io::stdin();
        loop {
            let mut input = String::new();
            if stdin.read_line(&mut input).is_ok() {
                if button_event_tx.send(Event::ButtonPress).is_err() {
                    break;
                }
            }
        }
    });

    // Main state machine thread
    let main_state_tx = state_tx.clone();
    thread::spawn(move || {
        let mut state = SystemState::new();
        let mut last_transition = Instant::now();
        
        // Send initial state
        let _ = main_state_tx.send(state.clone());
        
        for event in event_rx {
            let now = Instant::now();
            let mut state_changed = false;
            
            match event {
                Event::Timer => {
                    // Handle yellow to red transition (3 seconds)
                    if let Some(yellow_start) = state.yellow_timer_start {
                        if now.duration_since(yellow_start) >= Duration::from_secs(3) {
                            state.stoplight = StoplightState::Red;
                            state.crosswalk = CrosswalkState::Walk;
                            state.yellow_timer_start = None;
                            last_transition = now;
                            state_changed = true;
                        }
                    }
                    
                    // Handle button press timer (5 seconds from button press to yellow)
                    if let Some(button_start) = state.button_timer_start {
                        if now.duration_since(button_start) >= Duration::from_secs(5) {
                            if state.stoplight == StoplightState::Green {
                                state.stoplight = StoplightState::Yellow;
                                state.crosswalk = CrosswalkState::DontWalk;
                                state.yellow_timer_start = Some(now);
                                state.button_timer_start = None;
                                state.button_pressed = false;
                                state_changed = true;
                            }
                        }
                    }
                    
                    // Regular 10-second transitions
                    if state.yellow_timer_start.is_none() && 
                       state.button_timer_start.is_none() &&
                       now.duration_since(last_transition) >= Duration::from_secs(10) {
                        
                        match state.stoplight {
                            StoplightState::Red => {
                                state.stoplight = StoplightState::Green;
                                state.crosswalk = CrosswalkState::DontWalk;
                                last_transition = now;
                                state_changed = true;
                            }
                            StoplightState::Green => {
                                state.stoplight = StoplightState::Yellow;
                                state.crosswalk = CrosswalkState::DontWalk;
                                state.yellow_timer_start = Some(now);
                                state_changed = true;
                            }
                            StoplightState::Yellow => {
                                // This should be handled by the yellow timer above
                            }
                        }
                    }
                }
                
                Event::ButtonPress => {
                    if state.stoplight == StoplightState::Green && 
                       state.button_timer_start.is_none() && 
                       !state.button_pressed {
                        state.button_pressed = true;
                        state.button_timer_start = Some(now);
                        state_changed = true;
                    }
                }
            }
            
            if state_changed {
                let _ = main_state_tx.send(state.clone());
            }
        }
    });

    // Display thread
    for state in state_rx {
        state.display();
    }
}
