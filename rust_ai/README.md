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

## Second Prompt
I *love* how [Claude]() generated the state machines! Next I will extend the
prompt to:
* include a button which, when pressed, transitions the stoplight to YELLOW
after 5 seconds when it is GREEN, otherwise it does nothing,
* make the YELLOW to RED transition a fixed time of 3 seconds,
* change the printed emoji icons to pure text.

Follow the same steps for [First Prompt]() replacing the prompt with:
```
Write a Rust program with three threads communicating channels. One thread for a stop light state machine, one a synchronized crosswalk state machine and one thread to generate timer events to transition the stoplight and crosswalk every 10 seconds. 

The YELLOW to RED transition is a fixed time of 3 seconds.

The stoplight has a BUTTON event that will start the GREEN to YELLOW transition in 5 seconds.

The program will write only ascii text output.
```

This worked but had two problems.

First, the emojis were still being printed out so I replied 
```
Good program.  Change the print calls to use ascii text
```
and saw that the emojis were replaced with text strings.

Second, there was a simple compiler error [E0599]() which I diagnosed and fixed
by hand.

Now the program works well, including using standard in for the BUTTON,
transitioning only in the GREEN state.

WOW, very impressive!

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
  directory. Review this document to better understand Claude's **thinking**!

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

As [Claude]() points it, it `can make mistakes`, including [Compile Errors]().

[Rust]() has great support for resolving compile-time errors.  If these occur 
* use `rustc --explain` to analyze and to fix by hand in `main.rs`,
* or modify the prompt in an effort to avoid compile-time errors (but this is
  tricky!)

* Run the program
```
shell> ./target/debug/stop_cross
```

## Compile Errors

### E0599
* `error[E0599]: no method named `clone` found for struct `SystemState` in the current scope`

This is because `struct SystemState` does not have a `Clone` trait.  Add this by hand.
See https://doc.rust-lang.org/rust-by-example/trait/derive.html


