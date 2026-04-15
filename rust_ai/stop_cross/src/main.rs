use std::io::{self, BufRead};
use std::sync::mpsc::{self, Receiver, Sender};
use std::thread;
use std::time::{Duration, SystemTime, UNIX_EPOCH};

// ---------------------------------------------------------------------------
// State and Event enumerations
// ---------------------------------------------------------------------------

#[derive(Debug, Clone, Copy, PartialEq)]
enum State {
    Init,
    // Stoplight states
    Red,
    Green,
    Yellow,
    // Crosswalk states
    Walk,
    Blinking,
    DontWalk,
}

impl std::fmt::Display for State {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let s = match self {
            State::Init     => "INIT",
            State::Red      => "RED",
            State::Green    => "GREEN",
            State::Yellow   => "YELLOW",
            State::Walk     => "WALK",
            State::Blinking => "BLINKING",
            State::DontWalk => "DONT-WALK",
        };
        write!(f, "{}", s)
    }
}

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
}

impl std::fmt::Display for Event {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let s = match self {
            Event::Start    => "START",
            Event::Timer    => "TIMER",
            Event::Button   => "BUTTON",
            Event::Walk     => "WALK",
            Event::Blinking => "BLINKING",
            Event::DontWalk => "DONT-WALK",
            Event::Display  => "DISPLAY",
            Event::Exit     => "EXIT",
        };
        write!(f, "{}", s)
    }
}

// ---------------------------------------------------------------------------
// FSM framework — data-driven per OMG UML Chapter 14
// ---------------------------------------------------------------------------

// Entry/exit actions receive the event-router sender for cross-FSM communication.
type ActionFn = Box<dyn Fn(&Sender<(String, Event)>) + Send>;

/// A single row in the FSM transition table.
struct Transition {
    from_state:   State,
    event:        Event,
    exit_action:  Option<ActionFn>,
    to_state:     State,
    entry_action: Option<ActionFn>,
    description:  String,
}

/// The FSM structure as specified by the PRD.
struct Fsm {
    name:          String,
    current_state: State,
    last_event:    Option<Event>,
    transitions:   Vec<Transition>,
}

fn timestamp() -> String {
    let now = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default();
    let secs = now.as_secs() % 86400;
    let ms   = now.subsec_millis();
    format!("{:02}:{:02}:{:02}.{:03}",
        secs / 3600, (secs % 3600) / 60, secs % 60, ms)
}

impl Fsm {
    fn new(name: &str, transitions: Vec<Transition>) -> Self {
        Fsm {
            name: name.to_string(),
            current_state: State::Init,
            last_event: None,
            transitions,
        }
    }

    /// Process one event.  Returns false if the FSM should stop.
    fn process_event(&mut self, event: Event, router_tx: &Sender<(String, Event)>) -> bool {
        if event == Event::Exit {
            return false;
        }

        // Display is a meta-command, not a real transition.
        if event == Event::Display {
            let last = self.last_event
                .map_or("NONE".to_string(), |e| e.to_string());
            println!("  {:10} state={:10}  last_event={}", self.name, self.current_state.to_string(), last);
            return true;
        }

        // Find the first matching transition: current state AND event must match.
        let idx = self.transitions.iter().position(|t| {
            t.from_state == self.current_state && t.event == event
        });

        if let Some(i) = idx {
            // Print timestamp, FSM name, and descriptive text at transition time.
            println!("{} [{}] {}", timestamp(), self.name, self.transitions[i].description);

            // Execute exit action (leaving old state).
            if let Some(ref f) = self.transitions[i].exit_action {
                f(router_tx);
            }

            // Move to new state.
            self.current_state = self.transitions[i].to_state;
            self.last_event    = Some(event);

            // Execute entry action (entering new state).
            if let Some(ref f) = self.transitions[i].entry_action {
                f(router_tx);
            }
        } else {
            // Invalid event for current state — silently discard, but record it.
            self.last_event = Some(event);
        }

        true
    }
}

// ---------------------------------------------------------------------------
// Thread functions
// ---------------------------------------------------------------------------

/// Runs an FSM in its own thread, reading from `rx`.
fn fsm_thread(mut fsm: Fsm, rx: Receiver<Event>, router_tx: Sender<(String, Event)>) {
    for event in rx {
        if !fsm.process_event(event, &router_tx) {
            break;
        }
    }
}

/// Sends a TIMER event to all FSMs every `interval`.
fn timer_service(router_tx: Sender<(String, Event)>, interval: Duration) {
    loop {
        thread::sleep(interval);
        if router_tx.send(("Stoplight".to_string(), Event::Timer)).is_err() { break; }
        if router_tx.send(("Crosswalk".to_string(), Event::Timer)).is_err() { break; }
    }
}

/// Routes (target_name, event) pairs to the correct FSM channel.
fn event_router(
    rx:           Receiver<(String, Event)>,
    stoplight_tx: Sender<Event>,
    crosswalk_tx: Sender<Event>,
) {
    for (target, event) in rx {
        let result = match target.as_str() {
            "Stoplight" => stoplight_tx.send(event),
            "Crosswalk" => crosswalk_tx.send(event),
            _           => Ok(()),
        };
        if result.is_err() {
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// Transition table builders
// ---------------------------------------------------------------------------

fn build_stoplight_transitions(router_tx: Sender<(String, Event)>) -> Vec<Transition> {
    // Closure factory: sends WALK then schedules BLINKING 6 s later.
    let make_red_entry = |tx: Sender<(String, Event)>| -> ActionFn {
        Box::new(move |_: &Sender<(String, Event)>| {
            let _ = tx.send(("Crosswalk".to_string(), Event::Walk));
            let tx2 = tx.clone();
            thread::spawn(move || {
                // 6 seconds into RED → 4 seconds before the 10 s TIMER fires.
                thread::sleep(Duration::from_secs(6));
                let _ = tx2.send(("Crosswalk".to_string(), Event::Blinking));
            });
        })
    };

    let tx1 = router_tx.clone(); // INIT -> RED entry
    let tx2 = router_tx.clone(); // RED  -> GREEN entry
    let tx3 = router_tx.clone(); // YELLOW -> RED entry

    vec![
        Transition {
            from_state:   State::Init,
            event:        Event::Start,
            exit_action:  None,
            to_state:     State::Red,
            entry_action: Some(make_red_entry(tx1)),
            description:  "INIT -> RED on START".to_string(),
        },
        Transition {
            from_state:   State::Red,
            event:        Event::Timer,
            exit_action:  None,
            to_state:     State::Green,
            entry_action: Some(Box::new(move |_| {
                let _ = tx2.send(("Crosswalk".to_string(), Event::DontWalk));
            })),
            description:  "RED -> GREEN on TIMER".to_string(),
        },
        Transition {
            from_state:   State::Green,
            event:        Event::Timer,
            exit_action:  None,
            to_state:     State::Yellow,
            entry_action: None,
            description:  "GREEN -> YELLOW on TIMER".to_string(),
        },
        Transition {
            from_state:   State::Green,
            event:        Event::Button,
            exit_action:  None,
            to_state:     State::Yellow,
            entry_action: None,
            description:  "GREEN -> YELLOW on BUTTON (early crosswalk request)".to_string(),
        },
        Transition {
            from_state:   State::Yellow,
            event:        Event::Timer,
            exit_action:  None,
            to_state:     State::Red,
            entry_action: Some(make_red_entry(tx3)),
            description:  "YELLOW -> RED on TIMER".to_string(),
        },
    ]
}

fn build_crosswalk_transitions() -> Vec<Transition> {
    vec![
        Transition {
            from_state:   State::Init,
            event:        Event::Start,
            exit_action:  None,
            to_state:     State::DontWalk,
            entry_action: None,
            description:  "INIT -> DONT-WALK on START".to_string(),
        },
        Transition {
            from_state:   State::DontWalk,
            event:        Event::Walk,
            exit_action:  None,
            to_state:     State::Walk,
            entry_action: None,
            description:  "DONT-WALK -> WALK on WALK".to_string(),
        },
        Transition {
            from_state:   State::Walk,
            event:        Event::Blinking,
            exit_action:  None,
            to_state:     State::Blinking,
            entry_action: None,
            description:  "WALK -> BLINKING on BLINKING".to_string(),
        },
        Transition {
            from_state:   State::Blinking,
            event:        Event::DontWalk,
            exit_action:  None,
            to_state:     State::DontWalk,
            entry_action: None,
            description:  "BLINKING -> DONT-WALK on DONT-WALK".to_string(),
        },
    ]
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

fn main() {
    println!("Traffic Light / Crosswalk FSM Simulation");
    println!("Commands (whitespace-separated, case-insensitive):");
    println!("  S = Start   B = Button   D = Display   X = Exit");
    println!();

    // Central event router channel — all cross-FSM events flow through here.
    let (router_tx, router_rx) = mpsc::channel::<(String, Event)>();

    // Per-FSM direct channels.
    let (stoplight_tx, stoplight_rx) = mpsc::channel::<Event>();
    let (crosswalk_tx, crosswalk_rx) = mpsc::channel::<Event>();

    // Spawn event router thread.
    {
        let sl_tx = stoplight_tx.clone();
        let cw_tx = crosswalk_tx.clone();
        thread::spawn(move || event_router(router_rx, sl_tx, cw_tx));
    }

    // Spawn FSM threads.
    let sl_handle = {
        let fsm         = Fsm::new("Stoplight", build_stoplight_transitions(router_tx.clone()));
        let router_copy = router_tx.clone();
        thread::spawn(move || fsm_thread(fsm, stoplight_rx, router_copy))
    };

    let cw_handle = {
        let fsm         = Fsm::new("Crosswalk", build_crosswalk_transitions());
        let router_copy = router_tx.clone();
        thread::spawn(move || fsm_thread(fsm, crosswalk_rx, router_copy))
    };

    // Spawn timer service thread.
    {
        let tx = router_tx.clone();
        thread::spawn(move || timer_service(tx, Duration::from_secs(10)));
    }

    // Main thread: read whitespace-separated event tokens from stdin.
    let stdin = io::stdin();
    'input: for line in stdin.lock().lines() {
        let line = match line {
            Ok(l)  => l,
            Err(_) => break,
        };

        for token in line.split_whitespace() {
            match token.to_uppercase().as_str() {
                "S" => {
                    let _ = stoplight_tx.send(Event::Start);
                    let _ = crosswalk_tx.send(Event::Start);
                }
                "B" => {
                    let _ = stoplight_tx.send(Event::Button);
                }
                "D" => {
                    println!("--- Current FSM State ---");
                    let _ = stoplight_tx.send(Event::Display);
                    let _ = crosswalk_tx.send(Event::Display);
                }
                "X" => {
                    let _ = stoplight_tx.send(Event::Exit);
                    let _ = crosswalk_tx.send(Event::Exit);
                    break 'input;
                }
                _ => {} // All other tokens are silently ignored.
            }
        }
    }

    // Wait for FSM threads to finish.
    let _ = sl_handle.join();
    let _ = cw_handle.join();

    println!("Program terminated.");
}
