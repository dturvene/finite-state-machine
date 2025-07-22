I'll create a Rust program with three threads that implement a stoplight system with synchronized crosswalk signals. The threads will communicate using channels to coordinate state transitions.This Rust program implements a traffic control system with three communicating threads:

**Key Features:**

1. **Timer Thread**: Generates `TimerEvent::Tick` every 10 seconds to drive state transitions
2. **Stoplight Thread**: Implements a state machine with Red → Green → Yellow → Red cycle
3. **Crosswalk Thread**: Synchronizes with stoplight states (Walk when Red, Flashing when Yellow, Don't Walk when Green)

**Communication Channels:**
- Timer → Stoplight: Timer events
- Stoplight → Crosswalk: Current stoplight state
- Both state machines → Main: Status updates for display

**State Machines:**
- **Stoplight**: Red (traffic stops) → Green (traffic goes) → Yellow (prepare to stop) → Red
- **Crosswalk**: Walk (safe to cross) ↔ Flashing (finish crossing) ↔ Don't Walk (wait)

**Thread Safety:**
- Uses `mpsc` channels for safe inter-thread communication
- Proper error handling for when threads disconnect
- Graceful shutdown when channels close

To run this program, save it as `main.rs` and execute:
```bash
cargo run
```

The program will display real-time status updates showing both the traffic light state and corresponding crosswalk signals, with visual icons to make the current state clear. Each state transition happens every 10 seconds as specified.
