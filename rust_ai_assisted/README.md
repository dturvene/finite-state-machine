
# Prompt Engineering with Claude Sonnet 4

## First Prompt
* Start [Claude AI](https://claude.ai/new) session in your favorite browser.  I
  use [Google Chrome](https://www.google.com/chrome/)
* Choose [Claude Sonnet 4](https://www.anthropic.com/claude/sonnet) which is
  free for limited queries.
* enter a simple prompt:
```
Write a Rust program with three threads communicating with channels.  One thread for a stop light state machine, one a synchronized crosswalk state machine and one thread to generate timer events to transition the stoplight and crosswalk every 10 seconds.
```
* [Claude AI]() processes the prompt and generates two frames in the browser
  window:
  
  1. The left frame is a high level design (HLD) describing the rust program
     and some tips on how to build and run it.
  2. The right frame is a complete program implementing the HLD in
     [Rust](https://www.rust-lang.org/)
	 
* Click `Copy` in the left frame and save it to `HLD.md` in this directory
* Set up the rust code base:

- Create a new rust package `shell> cargo new stop-cross`
- click `Copy` in the right frame and paste it into `stop-cross/src/main.rs`

* Build the program `shell> cargo build`.  [Claude]() `can make mistakes`,
  including compile errors.  [Rust]() has great support for resolving
  compile-time errors.  If these occur either change the prompt or try to fix
  by hand in `main.rs`.
  
* Run the program: `shell> ./target/debug/stop-cross`.

This *should* create a program with functioning stoplight and crosswalk state
machines and a main function that demonstrates their interactions on standard
out.

* 
