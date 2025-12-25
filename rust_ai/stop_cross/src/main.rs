use std::sync::{Arc, Mutex, mpsc};
use std::thread;
use std::time::{Duration, SystemTime, UNIX_EPOCH};
use std::io;

#[derive(Debug, Clone, Copy, PartialEq)]
enum Event {
    Start,
    Timer,
    Button,
    Walk,
    Blinking,
    DontWalk,
    Exit,
    None,
}

#[derive(Debug, Clone, Copy, PartialEq)]
enum StoplightState {
    Init,
    Red,
    Yellow,
    Green,
}

#[derive(Debug, Clone, Copy, PartialEq)]
enum CrosswalkState {
    Init,
    Walk,
    Blinking,
    DontWalk,
}

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

type ActionFn<S> = Box<dyn Fn(&FSM<S>, &mpsc::Sender<(String, Event)>) + Send + Sync>;

struct Transition<S: State> {
    from_state: S,
    event: Event,
    exit_action: Option<ActionFn<S>>,
    to_state: S,
    entry_action: Option<ActionFn<S>>,
    description: String,
}

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
        let mut current_state_guard = self.current_state.lock().unwrap();
        let _current_state = *current_state_guard;
        *self.last_event.lock().unwrap() = event;

        if event == Event::Exit {
            return false;
        }

        let current_state = *current_state_guard;
        
        for transition in &self.transitions {
            if transition.from_state == current_state && transition.event == event {
                if let Some(exit_action) = &transition.exit_action {
                    exit_action(self, event_sender);
                }

                *current_state_guard = transition.to_state;

                print_transition(&self.name, &transition.description);

                if let Some(entry_action) = &transition.entry_action {
                    entry_action(self, event_sender);
                }

                return true;
            }
        }
        true
    }

    fn get_current_state(&self) -> S {
        *self.current_state.lock().unwrap()
    }

    fn get_last_event(&self) -> Event {
        *self.last_event.lock().unwrap()
    }
}

fn get_timestamp() -> String {
    let now = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap();
    let secs = now.as_secs() % 86400; // seconds in day
    let millis = now.subsec_millis();
    format!("{:02}:{:02}:{:02}.{:03}", 
        secs / 3600, (secs % 3600) / 60, secs % 60, millis)
}

fn print_transition(fsm_name: &str, description: &str) {
    println!("{} {} {}", get_timestamp(), fsm_name, description);
}

fn create_stoplight_fsm(_event_sender: mpsc::Sender<(String, Event)>) -> FSM<StoplightState> {
    let transitions = vec![
        Transition {
            from_state: StoplightState::Init,
            event: Event::Start,
            exit_action: None,
            to_state: StoplightState::Red,
            entry_action: Some(Box::new(move |_fsm, sender| {
                let _ = sender.send(("Crosswalk".to_string(), Event::Walk));
                
                let sender_clone = sender.clone();
                thread::spawn(move || {
                    thread::sleep(Duration::from_secs(6));
                    let _ = sender_clone.send(("Crosswalk".to_string(), Event::Blinking));
                });
            })),
            description: "Start -> Red state".to_string(),
        },
        Transition {
            from_state: StoplightState::Red,
            event: Event::Timer,
            exit_action: None,
            to_state: StoplightState::Green,
            entry_action: Some(Box::new(move |_fsm, sender| {
                let _ = sender.send(("Crosswalk".to_string(), Event::DontWalk));
            })),
            description: "Red -> Green state (Timer)".to_string(),
        },
        Transition {
            from_state: StoplightState::Green,
            event: Event::Button,
            exit_action: None,
            to_state: StoplightState::Yellow,
            entry_action: None,
            description: "Green -> Yellow state (Button)".to_string(),
        },
        Transition {
            from_state: StoplightState::Green,
            event: Event::Timer,
            exit_action: None,
            to_state: StoplightState::Yellow,
            entry_action: None,
            description: "Green -> Yellow state (Timer)".to_string(),
        },
        Transition {
            from_state: StoplightState::Yellow,
            event: Event::Timer,
            exit_action: None,
            to_state: StoplightState::Red,
            entry_action: Some(Box::new(move |_fsm, sender| {
                let _ = sender.send(("Crosswalk".to_string(), Event::Walk));
                
                let sender_clone = sender.clone();
                thread::spawn(move || {
                    thread::sleep(Duration::from_secs(6));
                    let _ = sender_clone.send(("Crosswalk".to_string(), Event::Blinking));
                });
            })),
            description: "Yellow -> Red state (Timer)".to_string(),
        },
    ];

    FSM::new("Stoplight".to_string(), transitions)
}

fn create_crosswalk_fsm() -> FSM<CrosswalkState> {
    let transitions = vec![
        Transition {
            from_state: CrosswalkState::Init,
            event: Event::Start,
            exit_action: None,
            to_state: CrosswalkState::DontWalk,
            entry_action: None,
            description: "Start -> DontWalk state".to_string(),
        },
        Transition {
            from_state: CrosswalkState::DontWalk,
            event: Event::Walk,
            exit_action: None,
            to_state: CrosswalkState::Walk,
            entry_action: None,
            description: "DontWalk -> Walk state".to_string(),
        },
        Transition {
            from_state: CrosswalkState::Walk,
            event: Event::Blinking,
            exit_action: None,
            to_state: CrosswalkState::Blinking,
            entry_action: None,
            description: "Walk -> Blinking state".to_string(),
        },
        Transition {
            from_state: CrosswalkState::Blinking,
            event: Event::DontWalk,
            exit_action: None,
            to_state: CrosswalkState::DontWalk,
            entry_action: None,
            description: "Blinking -> DontWalk state".to_string(),
        },
    ];

    FSM::new("Crosswalk".to_string(), transitions)
}

fn fsm_thread<S: State + Send + 'static>(
    fsm: Arc<FSM<S>>, 
    receiver: mpsc::Receiver<Event>,
    event_sender: mpsc::Sender<(String, Event)>
) {
    loop {
        match receiver.recv() {
            Ok(event) => {
                if !fsm.process_event(event, &event_sender) {
                    break;
                }
            },
            Err(_) => break,
        }
    }
}

fn timer_service(stoplight_sender: mpsc::Sender<Event>, crosswalk_sender: mpsc::Sender<Event>) {
    loop {
        thread::sleep(Duration::from_secs(10));
        if stoplight_sender.send(Event::Timer).is_err() || 
           crosswalk_sender.send(Event::Timer).is_err() {
            break;
        }
    }
}

fn event_router(receiver: mpsc::Receiver<(String, Event)>, 
               stoplight_sender: mpsc::Sender<Event>,
               crosswalk_sender: mpsc::Sender<Event>) {
    loop {
        match receiver.recv() {
            Ok((target, event)) => {
                match target.as_str() {
                    "Stoplight" => {
                        if stoplight_sender.send(event).is_err() {
                            break;
                        }
                    },
                    "Crosswalk" => {
                        if crosswalk_sender.send(event).is_err() {
                            break;
                        }
                    },
                    _ => {},
                }
            },
            Err(_) => break,
        }
    }
}

fn main() {
    println!("Traffic Light FSM Simulation");
    println!("Commands: S(tart), B(utton), D(isplay), X(exit)");
    println!();

    let (event_tx, event_rx) = mpsc::channel();
    let (stoplight_tx, stoplight_rx) = mpsc::channel();
    let (crosswalk_tx, crosswalk_rx) = mpsc::channel();

    let stoplight_fsm = Arc::new(create_stoplight_fsm(event_tx.clone()));
    let crosswalk_fsm = Arc::new(create_crosswalk_fsm());

    let stoplight_fsm_clone = Arc::clone(&stoplight_fsm);
    let crosswalk_fsm_clone = Arc::clone(&crosswalk_fsm);
    
    let event_tx_clone1 = event_tx.clone();
    let event_tx_clone2 = event_tx.clone();

    let stoplight_handle = thread::spawn(move || {
        fsm_thread(stoplight_fsm_clone, stoplight_rx, event_tx_clone1);
    });

    let crosswalk_handle = thread::spawn(move || {
        fsm_thread(crosswalk_fsm_clone, crosswalk_rx, event_tx_clone2);
    });

    let timer_tx1 = stoplight_tx.clone();
    let timer_tx2 = crosswalk_tx.clone();
    let timer_handle = thread::spawn(move || {
        timer_service(timer_tx1, timer_tx2);
    });

    let event_stoplight_tx = stoplight_tx.clone();
    let event_crosswalk_tx = crosswalk_tx.clone();
    let router_handle = thread::spawn(move || {
        event_router(event_rx, event_stoplight_tx, event_crosswalk_tx);
    });

    let stdin = io::stdin();
    loop {
        print!("> ");
        io::Write::flush(&mut io::stdout()).unwrap();
        
        let mut input = String::new();
        match stdin.read_line(&mut input) {
            Ok(_) => {
                let input = input.trim().to_uppercase();
                match input.as_str() {
                    "S" => {
                        let _ = stoplight_tx.send(Event::Start);
                        let _ = crosswalk_tx.send(Event::Start);
                    },
                    "B" => {
                        let _ = stoplight_tx.send(Event::Button);
                    },
                    "D" => {
                        println!("Stoplight: State={:?}, LastEvent={:?}", 
                                stoplight_fsm.get_current_state(), 
                                stoplight_fsm.get_last_event());
                        println!("Crosswalk: State={:?}, LastEvent={:?}", 
                                crosswalk_fsm.get_current_state(), 
                                crosswalk_fsm.get_last_event());
                    },
                    "X" => {
                        let _ = stoplight_tx.send(Event::Exit);
                        let _ = crosswalk_tx.send(Event::Exit);
                        break;
                    },
                    _ => {
                        println!("Invalid command. Use S, B, D, or X");
                    }
                }
            },
            Err(_) => break,
        }
    }

    drop(stoplight_tx);
    drop(crosswalk_tx);
    drop(event_tx);

    let _ = stoplight_handle.join();
    let _ = crosswalk_handle.join();
    let _ = timer_handle.join();
    let _ = router_handle.join();

    println!("Program terminated.");
}