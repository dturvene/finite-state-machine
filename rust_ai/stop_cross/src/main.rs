use std::sync::mpsc;
use std::thread;
use std::time::Duration;

#[derive(Debug, Clone, Copy)]
enum StoplightState {
    Red,
    Green,
    Yellow,
}

#[derive(Debug, Clone, Copy)]
enum CrosswalkState {
    DontWalk,
    Walk,
    Flashing,
}

#[derive(Debug)]
enum TimerEvent {
    Tick,
}

#[derive(Debug)]
enum StateUpdate {
    StoplightChanged(StoplightState),
    CrosswalkChanged(CrosswalkState),
}

fn main() {
    // Channels for communication between threads
    let (timer_tx, timer_rx) = mpsc::channel::<TimerEvent>();
    let (state_tx, state_rx) = mpsc::channel::<StateUpdate>();
    let (crosswalk_tx, crosswalk_rx) = mpsc::channel::<StoplightState>();

    // Clone senders for different threads
    let state_tx_stoplight = state_tx.clone();
    let state_tx_crosswalk = state_tx.clone();

    // Timer thread - generates tick events every 10 seconds
    let timer_handle = thread::spawn(move || {
        println!("Timer thread started");
        loop {
            thread::sleep(Duration::from_secs(10));
            if timer_tx.send(TimerEvent::Tick).is_err() {
                println!("Timer thread: Main thread disconnected, exiting");
                break;
            }
        }
    });

    // Stoplight thread - manages traffic light state machine
    let stoplight_handle = thread::spawn(move || {
        println!("Stoplight thread started");
        let mut current_state = StoplightState::Red;
        
        // Send initial state
        let _ = state_tx_stoplight.send(StateUpdate::StoplightChanged(current_state));
        let _ = crosswalk_tx.send(current_state);
        
        loop {
            match timer_rx.recv() {
                Ok(TimerEvent::Tick) => {
                    // State machine transitions
                    current_state = match current_state {
                        StoplightState::Red => StoplightState::Green,
                        StoplightState::Green => StoplightState::Yellow,
                        StoplightState::Yellow => StoplightState::Red,
                    };
                    
                    println!("Stoplight: Transitioning to {:?}", current_state);
                    
                    // Notify main thread and crosswalk thread
                    if state_tx_stoplight.send(StateUpdate::StoplightChanged(current_state)).is_err() {
                        println!("Stoplight thread: Main thread disconnected, exiting");
                        break;
                    }
                    
                    if crosswalk_tx.send(current_state).is_err() {
                        println!("Stoplight thread: Crosswalk thread disconnected, exiting");
                        break;
                    }
                }
                Err(_) => {
                    println!("Stoplight thread: Timer thread disconnected, exiting");
                    break;
                }
            }
        }
    });

    // Crosswalk thread - manages pedestrian crossing signals
    let crosswalk_handle = thread::spawn(move || {
        println!("Crosswalk thread started");
        let mut current_state = CrosswalkState::DontWalk;
        
        // Send initial state
        let _ = state_tx_crosswalk.send(StateUpdate::CrosswalkChanged(current_state));
        
        loop {
            match crosswalk_rx.recv() {
                Ok(stoplight_state) => {
                    // Crosswalk state synchronized with stoplight
                    current_state = match stoplight_state {
                        StoplightState::Red => CrosswalkState::Walk,
                        StoplightState::Yellow => CrosswalkState::Flashing,
                        StoplightState::Green => CrosswalkState::DontWalk,
                    };
                    
                    println!("Crosswalk: Transitioning to {:?} (stoplight is {:?})", 
                             current_state, stoplight_state);
                    
                    if state_tx_crosswalk.send(StateUpdate::CrosswalkChanged(current_state)).is_err() {
                        println!("Crosswalk thread: Main thread disconnected, exiting");
                        break;
                    }
                }
                Err(_) => {
                    println!("Crosswalk thread: Stoplight thread disconnected, exiting");
                    break;
                }
            }
        }
    });

    // Main thread - receives and displays state updates
    println!("Main thread: System started");
    println!("Initial states:");
    
    let mut stoplight_state = StoplightState::Red;
    let mut crosswalk_state = CrosswalkState::DontWalk;
    
    loop {
        match state_rx.recv() {
            Ok(StateUpdate::StoplightChanged(state)) => {
                stoplight_state = state;
                println!("\n🚦 STOPLIGHT: {:?}", state);
                display_system_status(stoplight_state, crosswalk_state);
            }
            Ok(StateUpdate::CrosswalkChanged(state)) => {
                crosswalk_state = state;
                println!("🚶 CROSSWALK: {:?}", state);
                display_system_status(stoplight_state, crosswalk_state);
            }
            Err(_) => {
                println!("Main thread: All worker threads disconnected, shutting down");
                break;
            }
        }
    }

    // Wait for threads to finish
    let _ = timer_handle.join();
    let _ = stoplight_handle.join();
    let _ = crosswalk_handle.join();
    
    println!("System shutdown complete");
}

fn display_system_status(stoplight: StoplightState, crosswalk: CrosswalkState) {
    let stoplight_icon = match stoplight {
        StoplightState::Red => "🔴",
        StoplightState::Yellow => "🟡",
        StoplightState::Green => "🟢",
    };
    
    let crosswalk_icon = match crosswalk {
        CrosswalkState::Walk => "🚶‍♂️✅",
        CrosswalkState::DontWalk => "🚶‍♂️❌",
        CrosswalkState::Flashing => "🚶‍♂️⚠️",
    };
    
    println!("Status: {} Traffic | {} Pedestrians", stoplight_icon, crosswalk_icon);
    println!("----------------------------------------");
}
