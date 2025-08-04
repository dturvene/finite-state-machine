use std::sync::{Arc, Mutex, mpsc};
use std::thread;
use std::time::{Duration, SystemTime, UNIX_EPOCH};
use std::io::{self, BufRead};

// Event types
#[derive(Debug, Clone, Copy, PartialEq)]
enum Event {
    Start,
    Timer,
    Button,
    Walk,
    Blinking,
    DontWalk,
    Display,
    Exit,
    None,
}

// States for Stoplight FSM
#[derive(Debug, Clone, Copy, PartialEq)]
enum StoplightState {
    Init,
    Red,
    Yellow,
    Green,
}

// States for Crosswalk FSM
#[derive(Debug, Clone, Copy, PartialEq)]
enum CrosswalkState {
    Init,
    Walk,
    Blinking,
    DontWalk,
}

// Generic state trait
trait State: Clone + Copy + PartialEq + std::fmt::Debug {
    fn init() -> Self;
}

impl State for StoplightState {
    fn init() -> Self {
        StoplightState::Init
    }
}

impl State for CrosswalkState {
    fn init() -> Self {
        CrosswalkState::Init
    }
}

// Action function type
type ActionFn<S> = Box<dyn Fn(&FSM<S>, &mpsc::Sender<(String, Event)>) + Send + Sync>;

// Transition structure - now includes from_state
struct Transition<S: State> {
    from_state: S,
    event: Event,
    exit_action: Option<ActionFn<S>>,
    to_state: S,
    entry_action: Option<ActionFn<S>>,
    description: String,
}

// FSM structure
struct FSM<S: State> {
    name: String,
    current_state: Arc<Mutex<S>>,
    last_event: Arc<Mutex<Event>>,
    transitions: Vec<Transition<S>>,
}

impl<S: State + 'static> FSM<S> {
    fn new(name: String, transitions: Vec<Transition<S>>) -> Self {
        FSM {
            name,
            current_state: Arc::new(Mutex::new(S::init())),
            last_event: Arc::new(Mutex::new(Event::None)),
            transitions,
        }
    }

    fn process_event(&self, event: Event, event_sender: &mpsc::Sender<(String, Event)>) -> bool {
        let current_state = *self.current_state.lock().unwrap();
        *self.last_event.lock().unwrap() = event;

        // Find matching transition
        for transition in &self.transitions {
            if transition.from_state == current_state && transition.event == event {
                // Execute exit action
                if let Some(exit_action) = &transition.exit_action {
                    exit_action(self, event_sender);
                }

                // Execute entry action
                if let Some(entry_action) = &transition.entry_action {
                    entry_action(self, event_sender);
                }

                print_transition(&self.name, &transition.description);
                *self.current_state.lock().unwrap() = transition.to_state;
                return true;
            }
        }

        false // Event not handled (silently discarded)
    }

    fn display_state(&self) {
        let state = *self.current_state.lock().unwrap();
        let event = *self.last_event.lock().unwrap();
        println!("{}: State={:?}, Last Event={:?}", self.name, state, event);
    }
}

fn print_transition(fsm_name: &str, description: &str) {
    let now = SystemTime::now().duration_since(UNIX_EPOCH).unwrap();
    let secs = now.as_secs();
    let millis = now.subsec_millis();
    println!("[{}.{:03}] {}: {}", secs % 1000, millis, fsm_name, description);
}

fn create_stoplight_fsm() -> FSM<StoplightState> {
    let transitions = vec![
        // Init -> Red (Start event)
        Transition {
            from_state: StoplightState::Init,
            event: Event::Start,
            exit_action: None,
            to_state: StoplightState::Red,
            entry_action: Some(Box::new(|_fsm, event_sender| {
                event_sender.send(("Crosswalk".to_string(), Event::Walk)).unwrap();
                // Schedule blinking event after 6 seconds (10 - 4)
                let sender = event_sender.clone();
                thread::spawn(move || {
                    thread::sleep(Duration::from_secs(6));
                    sender.send(("Crosswalk".to_string(), Event::Blinking)).unwrap();
                });
            })),
            description: "Transition to RED".to_string(),
        },
        // Red -> Green (Timer event)
        Transition {
            from_state: StoplightState::Red,
            event: Event::Timer,
            exit_action: None,
            to_state: StoplightState::Green,
            entry_action: Some(Box::new(|_fsm, event_sender| {
                event_sender.send(("Crosswalk".to_string(), Event::DontWalk)).unwrap();
            })),
            description: "Transition from RED to GREEN".to_string(),
        },
        // Green -> Yellow (Button event)
        Transition {
            from_state: StoplightState::Green,
            event: Event::Button,
            exit_action: None,
            to_state: StoplightState::Yellow,
            entry_action: None,
            description: "Transition from GREEN to YELLOW (Button pressed)".to_string(),
        },
        // Green -> Yellow (Timer event)
        Transition {
            from_state: StoplightState::Green,
            event: Event::Timer,
            exit_action: None,
            to_state: StoplightState::Yellow,
            entry_action: None,
            description: "Transition from GREEN to YELLOW".to_string(),
        },
        // Yellow -> Red (Timer event)
        Transition {
            from_state: StoplightState::Yellow,
            event: Event::Timer,
            exit_action: None,
            to_state: StoplightState::Red,
            entry_action: Some(Box::new(|_fsm, event_sender| {
                event_sender.send(("Crosswalk".to_string(), Event::Walk)).unwrap();
                // Schedule blinking event after 6 seconds
                let sender = event_sender.clone();
                thread::spawn(move || {
                    thread::sleep(Duration::from_secs(6));
                    sender.send(("Crosswalk".to_string(), Event::Blinking)).unwrap();
                });
            })),
            description: "Transition from YELLOW to RED".to_string(),
        },
    ];

    FSM::new("Stoplight".to_string(), transitions)
}

fn create_crosswalk_fsm() -> FSM<CrosswalkState> {
    let transitions = vec![
        // Init -> DontWalk (Start event)
        Transition {
            from_state: CrosswalkState::Init,
            event: Event::Start,
            exit_action: None,
            to_state: CrosswalkState::DontWalk,
            entry_action: None,
            description: "Transition to DONT-WALK".to_string(),
        },
        // DontWalk -> Walk (Walk event)
        Transition {
            from_state: CrosswalkState::DontWalk,
            event: Event::Walk,
            exit_action: None,
            to_state: CrosswalkState::Walk,
            entry_action: None,
            description: "Transition from DONT-WALK to WALK".to_string(),
        },
        // Walk -> Blinking (Blinking event)
        Transition {
            from_state: CrosswalkState::Walk,
            event: Event::Blinking,
            exit_action: None,
            to_state: CrosswalkState::Blinking,
            entry_action: None,
            description: "Transition from WALK to BLINKING".to_string(),
        },
        // Blinking -> DontWalk (DontWalk event)
        Transition {
            from_state: CrosswalkState::Blinking,
            event: Event::DontWalk,
            exit_action: None,
            to_state: CrosswalkState::DontWalk,
            entry_action: None,
            description: "Transition from BLINKING to DONT-WALK".to_string(),
        },
        // Walk -> DontWalk (DontWalk event - direct transition)
        Transition {
            from_state: CrosswalkState::Walk,
            event: Event::DontWalk,
            exit_action: None,
            to_state: CrosswalkState::DontWalk,
            entry_action: None,
            description: "Transition from WALK to DONT-WALK".to_string(),
        },
    ];

    FSM::new("Crosswalk".to_string(), transitions)
}

fn timer_service(event_sender: mpsc::Sender<(String, Event)>) {
    loop {
        thread::sleep(Duration::from_secs(10));
        if event_sender.send(("Stoplight".to_string(), Event::Timer)).is_err() {
            break;
        }
    }
}

fn fsm_thread<S: State + Send + 'static>(
    fsm: Arc<FSM<S>>,
    event_receiver: mpsc::Receiver<Event>,
    event_sender: mpsc::Sender<(String, Event)>,
) {
    loop {
        match event_receiver.recv() {
            Ok(event) => {
                match event {
                    Event::Exit => break,
                    Event::Display => fsm.display_state(),
                    _ => {
                        fsm.process_event(event, &event_sender);
                    }
                }
            }
            Err(_) => break,
        }
    }
}

fn main() {
    // Create event channels
    let (main_event_sender, main_event_receiver) = mpsc::channel::<(String, Event)>();
    let (stoplight_sender, stoplight_receiver) = mpsc::channel::<Event>();
    let (crosswalk_sender, crosswalk_receiver) = mpsc::channel::<Event>();
    
    // Create FSMs
    let stoplight_fsm = Arc::new(create_stoplight_fsm());
    let crosswalk_fsm = Arc::new(create_crosswalk_fsm());
    
    // Clone for threads
    let stoplight_fsm_clone = stoplight_fsm.clone();
    let crosswalk_fsm_clone = crosswalk_fsm.clone();
    let main_event_sender_clone1 = main_event_sender.clone();
    let main_event_sender_clone2 = main_event_sender.clone();
    let main_event_sender_clone3 = main_event_sender.clone();
    
    // Spawn FSM threads
    let stoplight_thread = thread::spawn(move || {
        fsm_thread(stoplight_fsm_clone, stoplight_receiver, main_event_sender_clone1);
    });
    
    let crosswalk_thread = thread::spawn(move || {
        fsm_thread(crosswalk_fsm_clone, crosswalk_receiver, main_event_sender_clone2);
    });
    
    // Spawn timer thread
    let _timer_thread = thread::spawn(move || {
        timer_service(main_event_sender_clone3);
    });
    
    // Event routing thread
    let stoplight_sender_clone = stoplight_sender.clone();
    let crosswalk_sender_clone = crosswalk_sender.clone();
    let event_router = thread::spawn(move || {
        while let Ok((target, event)) = main_event_receiver.recv() {
            match target.as_str() {
                "Stoplight" => {
                    if stoplight_sender_clone.send(event).is_err() {
                        break;
                    }
                }
                "Crosswalk" => {
                    if crosswalk_sender_clone.send(event).is_err() {
                        break;
                    }
                }
                _ => {}
            }
        }
    });
    
    // Read events from stdin
    println!("Enter events: S (start), B (button), D (display), X (exit)");
    let stdin = io::stdin();
    let reader = stdin.lock();
    
    for line in reader.lines() {
        if let Ok(line) = line {
            for token in line.split_whitespace() {
                match token {
                    "S" => {
                        stoplight_sender.send(Event::Start).unwrap();
                        crosswalk_sender.send(Event::Start).unwrap();
                    }
                    "B" => {
                        stoplight_sender.send(Event::Button).unwrap();
                    }
                    "D" => {
                        stoplight_sender.send(Event::Display).unwrap();
                        crosswalk_sender.send(Event::Display).unwrap();
                    }
                    "X" => {
                        stoplight_sender.send(Event::Exit).unwrap();
                        crosswalk_sender.send(Event::Exit).unwrap();
                        drop(main_event_sender);
                        drop(stoplight_sender);
                        drop(crosswalk_sender);
                        
                        // Wait for threads to finish
                        stoplight_thread.join().unwrap();
                        crosswalk_thread.join().unwrap();
                        event_router.join().unwrap();
                        
                        return;
                    }
                    _ => {} // Discard other text
                }
            }
        }
    }
}
