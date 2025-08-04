<!-- 
gitrepo.sh:github_rust_ai
;; proof-read:
pandoc README.md -o /tmp/readme.html
google-chrome /tmp/readme.html
-->

# Project Update
Subsequent to publishing this article on [Medium], I performed 
[Prompt 3](#prompt-3-using-prd-file) replacing the [Claude Sonnet 4] model with the
more advanced [Claude Opus 4] model, which is available with a pro
subscription.  The results were similar but Opus 4 seemed better at
interpreting the prompts.

See the `HLD.md` document for the prompt/response/reply exchange. My first
reply caused Claude to extensively refactor the code!  That would have taken me
at least hours to do.

I committed the work using the `ai_prompt4` tag.

# Prompt Engineering with Claude Sonnet 4
The goal of this effort is to use a current or `frontier` AI model to
create a executable from progressively more complex prompts.  The prompt
attempts to create a program for a classic stoplight with synchronized
crosswalk in the [Rust] language.

There are a **huge** number of agentic models to achieve our goal; I tested a
subset with varying degrees of success. For this paper I selected the free
version of [Claude AI] because it seemed to 
generate good Rust code and I liked the interface.  In the next phase I will 
explore AI-assisted Integrated Development Environments (IDEs), as opposed to
the agentic code I have been creating in this project.

[Claude AI] uses a browser window with a prompt input
area and several buttons for domain-specific prompts.  One can select a Claude
model to run the prompt but the free version uses only 
[Claude Sonnet 4].  After a coding prompt is entered, the browser window splits into
two panes: the right pane is the generated code and the left pane is a High
Level Description (HLD) of the code.

For the "prompt engineering" methodology I loosely followed 
[Claude Prompt Engineering]. In retrospect I should have followed the
recommendations more; I think it would have produced more accurate results. 
Lesson learned!

One issue I faced with the [Claude AI] free version was hitting
a daily limit on `free messages` when I entered too many prompts and prompt
replys.  I avoided this by hand-editting the code, but several times I hit the
usage token limit and just had to put this aside until the next day.

I performed three prompt cycles:

* [Prompt 1](#prompt-1): a simple prompt to assertain the capabilities of Claude
  without much effort. I was impressed!
* [Prompt 2](#prompt-2): a more detailed prompt to add requirements. The result was much
  better but also produced more issues.  Again very impressive!
* [Prompt 3](#prompt-3-using-prd-file): I tried to capture all the requirements
  of the [C] project. 
  This was more daunting and caused me to update the requirements 
  over several days.  However, the feedback cycle of requirement, code
  inspection, requirement update resulted in something similar to the 
  original project.
  
I have a working knowledge of Rust but no where near the level 
of comfort I have with C so I was grateful for all the automatic code
generation done by Claude.
  
In summary, it is clear AI Assisted and Agentic code development is the future.
The old development process was: copy-and-paste boiler plate code, add
libraries and subject matter expert (SME) code, compile, test, release.  The
new process is use an AI model to generate the code, tune it through prompts,
compile, test, release.

## Prompt 1
* Start [Claude AI] session in your favorite browser. I use [Google Chrome].
* Choose [Claude Sonnet 4] which is free for limited queries. 
* Enter a simple prompt:

```
Write a Rust program with three threads communicating with channels.  One thread for a stop light state machine, one a synchronized crosswalk state machine and one thread to generate timer events to transition the stoplight and crosswalk every 10 seconds.
```

This **should** create a program with functioning stoplight and crosswalk state
machines and a main function that prints their interactions on standard out.
PRETTY COOL!

The program will run continuously; to stop program with a SIGINT signal
(`Control-C`).

In this github repo I tagged this effort as `ai_prompt1`.

## Prompt 2
I *love* how Claude generated the state machines! Next I will extend the
prompt to:

* include a button which, when pressed, transitions the stoplight to YELLOW
  after 5 seconds when it is GREEN, otherwise it does nothing,
* make the YELLOW to RED transition a fixed time of 3 seconds,
* change the printed emoji icons to pure text.

Follow the same steps for `Prompt 1` replacing the prompt with:
```
Write a Rust program with three threads communicating channels. One thread for a stop light state machine, one a synchronized crosswalk state machine and one thread to generate timer events to transition the stoplight and crosswalk every 10 seconds. 

The YELLOW to RED transition is a fixed time of 3 seconds.

The stoplight has a BUTTON event that will start the GREEN to YELLOW transition in 5 seconds.

The program will write only ascii text output.
```

This worked but had two problems.

First, the emojis were still being printed out so I replied in the prompt area: 
```
Good program.  Change the print calls to use ascii text
```
and saw that the emojis were replaced with text strings.

Second, there was a simple compiler error [E0599](#e0599) which I
diagnosed and fixed by hand.

Now the program works well, including using standard in for the BUTTON,
transitioning the crosswalk to WALK only when the stoplight is in the GREEN
state.

WOW, very impressive!

I tagged this effort as `ai_prompt2`.

## Prompt 3 using PRD file
While testing the Claude capabilities, I began to create prompts that
were unwieldly in the prompt window.  I adopted the solution of putting the
prompt requirements in a version controlled file, `prd.stoplight-crosswalk.md`
and used a simple prompt to read the file.  See the `Prompt` section in the PRD
file.

The increased granularity of requirements in the PRD file caused Claude
to produce interesting, and sometimes baffling, code.  I suspect Claude 
matches a hueristic requirement to code fragement(s) and integrates into the
program source.  But sometimes the fragments are not germane.  For example,
one PRD version cause Claude to generate a Rust [HashMap] for a 
small set of transition structures.  I would need to write several HashMap
helper functions and clear more compiler errors to use it.  I clarified the
requirement to use a Rust [Vector].

The final PRD and subsequent reply modifications in `HLD.md` was the sixth
iteration of the PRD, and even then I needed two reply prompts to modify the
code to what I wanted. [Prompt engineering][claude-prompt-engineering] is a
discipline!

I tagged the final effort for this as `ai_prompt3`.

## Prompt Engineering Development Process
For each prompt example, take the following steps to save the design and rust
code.  This is a little cumbersome and where AI IDEs are valuable.  But I did
not want to add the learning curve of an IDE to this project.

* [Claude AI] processes the prompt and generates two frames in the browser
  window: 
  
  - The left frame is a high level design (HLD) describing the rust program
	and some tips on how to build and run it.
  - The right frame is a complete program implementing the HLD in
	[Rust]

### High Level Design Documentation
* Click `Copy` in the left frame and save it to `HLD.md` in this
  directory. Review this document to better understand Claude's **thinking**!
   
The Claude description is already formatted as markdown so I pasted it into
`HLD.md` with:

* a `PROMPT` header for the original promp
* a `RESPONSE` header for the original response generated by Claude

If I added subsequent prompt reply messages in the `Reply to Claude..` input
area, I copy the reply and response into sections labelled as `Reply N` and
`Response N` where `N` is a sequence number for the reply iteration.  Each
iteration will regenerate the entire code pane.

### Rust Code
* Create a new rust package in the current FSM github repo. Use the `--vcs none`
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

As Claude points it, it `can make mistakes`, including 
[Compile Errors](#compile-errors).

[Rust] has great support for resolving compile-time errors.  If these occur 
* use `rustc --explain` to analyze and to fix by hand in `main.rs`,
* or modify the prompt in an effort to avoid compile-time errors (but this is
  tricky!)

* Run the program
```
shell> ./target/debug/stop_cross
```

After the code compiles and runs as desired, I do the following:
* make sure `HLD.md` is up-to-date with the code,
* git commit all changes
* tag the commit with `ai_promptN` where `N` is the prompt number in this
  README. So the `Prompt 2` section is tagged as `ai_prompt2` 

# Compile Errors
This section contains a list of common compile errors and resolution.  By far
the most generated error/warning during this project is related to unused
functions, variables, imports.  I overrode the warnings rather than add a Clause
reply to remove the offending code in a new code gen, thus burning a lot of
usage tokens.

## E0599
* `error[E0599]: no method named `clone` found for struct `SystemState` in the current scope`

This is because `struct SystemState` does not have a derived `Clone` trait.
See https://doc.rust-lang.org/rust-by-example/trait/derive.html
Edit `main.rs` to add Clone trait:
```
#[derive(Debug, Clone)]
struct SystemState {
```

## E0282
* `error[E0282]: type annotations needed`

## E0382
```
error[E0382]: use of moved value: `event_rx`
```

`event_rx` is an `mpsc::channel` receiver instance defined in main.  It cannot
be used in another thread, and cannot be cloned.

Look at using `crossbeam_channel` for multiple readers, by cloning the
receiver.

Another E0382 error:
```
error[E0382]: borrow of moved value: `stoplight_sender_clone`
```

This value was defined previously in
```
let stoplight_sender_clone = stoplight_sender.clone();
```

and is used in a function here:
```
  timer_thread(stoplight_sender_clone.clone());
```

Fix by cloning the original instance:
```
  timer_thread(stoplight_senderclone());
``` 

## Warning: crossbeam_channel Receiver unused

```
warning: unused import: `Receiver`
```

The original `event_rx` is cloned but never explicitly used in the main
routine. Solution is to add at top of file:

```
#![allow(unused_imports)]
```

## Warning: unused variable

Possible solutions:
* Top of file: `#![allow(unused_variables)]`
* Above unused variable: `#[allow(unused_variables)]`
* prepend a `_` to the variable name

## E0506
```
error[E0506]: cannot assign to `self.current_state` because it is borrowed
```

This is a common complaint for those coming from [C].  It indicates that a
function is using a variable defined in another function (borrowing it) and
trying to assign the borrowed value to another variable. 

This is similiar to [E0382](#e0382). Code structured like this is tricky to
refactor!

## warning field is never read OR function is never used

This is a simple warning.  Rather than repair the warning,
dd this directive above the offending code fragment
```
#[allow(dead_code)]
```

## E0308 

```
error[E0308]: mismatched types
...
expected `&str`, found `String`
```

Fix following this link:
* https://users.rust-lang.org/t/whats-a-string-and-how-do-i-convert-it-into-a-str/92429

<!-- external reference links -->
[Rust]: https://www.rust-lang.org/
[Claude AI]: https://claude.ai/new
[Claude Sonnet 4]: https://www.anthropic.com/claude/sonnet
[Claude Opus 4]: https://www.anthropic.com/claude/opus
[Claude Prompt Engineering]: https://docs.anthropic.com/en/docs/build-with-claude/prompt-engineering/overview
[C]: https://www.c-language.org/
[Google Chrome]: https://www.google.com/chrome/
[Hashmap]: https://doc.rust-lang.org/std/collections/struct.HashMap.html
[Vector]: https://doc.rust-lang.org/std/vec/struct.Vec.html
[Medium]: https://medium.com/
