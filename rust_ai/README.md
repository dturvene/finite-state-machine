<!-- 
https://docs.github.com/en/get-started/writing-on-github/getting-started-with-writing-and-formatting-on-github/basic-writing-and-formatting-syntax
-->

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

This **should** create a program with functioning stoplight and crosswalk state
machines and a main function that prints their interactions on standard out.
PRETTY COOL!

The program will run continuously; to stop program with a SIGINT signal
(`Control-C`).

## Prompt Engineering Development Process
For each prompt example, take the following steps to save the design and rust code.

* [Claude AI]() processes the prompt and generates two frames in the browser
window:
  
  - The left frame is a high level design (HLD) describing the rust program
	and some tips on how to build and run it.
  - The right frame is a complete program implementing the HLD in
	[Rust](https://www.rust-lang.org/)

### High Level Design Documentation
* Click `Copy` in the left frame and save it to `HLD.md` in this
  directory. Review this document to better understand Claude's **thinking**

### Rust Code
* Create a new rust package in the current github repo. Use the `--vcs none`
  argument to prevent creating a git subproject.
  `shell> cargo new stop_cross --vcs none`
  
* click `Copy` in the browser right frame and paste the rust code into
  `stop_cross/src/main.rs`

* Clean and build the program:
```
shell> cd stop_cross
shell> cargo clean
shell> cargo build
```

As [Claude]() points it, it `can make mistakes`, including compile errors.

[Rust]() has great support for resolving compile-time errors.  If these occur 
* use `rustc --explain` to analyze and to fix by hand in `main.rs`,
* or modify the prompt in an effort to avoid compile-time errors (but this is
  tricky!)

* Run the program
```
shell> ./target/debug/stop_cross
```

