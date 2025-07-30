<!-- 
* convert to HTML to view
pandoc medium-rust-aiagent.md -o mra.html

* upload
https://github.com/dturvene/finite-state-machine/blob/main/rust_ai/medium-article.md

-->

# Abstract
This article

# Background
On May 11 2021 I published 
[Implementing a Finite State Machine in C](https://medium.com/@dturvene/implementing-a-finite-state-machine-in-c-aede8951b737)

The project "demonstrates how to create, manage, drive and regression test a
deterministic finites state machine (DFSM)."  It was based on my long-time
professional experience coding FSMs in large scale products for telecom
systems, which I implemented in 
[C](https://www.c-language.org/) and 
[C++](https://isocpp.org/)

Over decades I modified my implementation for an event driven FSM pattern.
Eventually absorbing some traits of the 
[OMG UML](https://www.omg.org/spec/UML/2.5.1) 
* chapter 14 `State Machines`,
* chapter 16 `Actions`, 
* section 13.3 `Events`.

Much of the concepts in the [OMG UML]() document was, for my purposes,
unnecessary. For example, the OMG State Machine defines a `guard transition`,
and THREE! kinds of states. The advantage to basing my FSMs on UML is the
FSM nomenclature is defined and documented.

My production FSMs were quite complex with thousands of instances running
concurrently. Furthermore, the incoming events were generated both in response
to FSM actions and asynchronously from hardware changes.  This required me to
simulate generating events to inject into the FSM instances.

The example in my 2021 paper was the very familiar pattern for a traffic
stoplight and synchronized crosswalk, including a pedestrian button.  The two
FSMs have enough states and events to demonstate the structures but simple
enough to understand comprehensively.

# C to Rust
Later in 2021 I started to learn [Rust](https://www.rust-lang.org/), coding
simple projects to understand the concepts. One goal I had in mind was to port
this project to Rust.

However, I put that work on the self in order to work on Machine Learning using 
[Tensorflow](https://www.tensorflow.org/), which primarily uses 
[python](https://www.python.org/).

As the ML models matured I was fascinated by what they could (and could not)
predict. I started experimenting with 
[time series sequences](https://en.wikipedia.org/wiki/Time_series) and
[time series forecasting](https://www.tensorflow.org/tutorials/structured_data/time_series) 
models training on historical data from
coinbase on a basket of crypto tokens to predict movement for a target
cryptotoken.  It was really cool stuff but the models I created and trained
never produced a reliable accuracy level for the evaluation data.  I was not
confident any would predict future price movements.  I invested a good amount
of time in researching and understanding the model layers and how they impacted
accuracy (sometimes wildly!) but I did not progress in adding/tuning layers to
increase the accuracy of the evaluation data.

So I shelved that effort also.

Around this time early 
[Wikipedia: LLM](https://en.wikipedia.org/wiki/Large_language_model) like 
[OpenAI ChatGPT](https://chatgpt.com/),
[Google Gemini](https://gemini.google.com/app) and
[Anthropic Claude](https://claude.ai/new)
started to emerge.  The results were wonderful (by definition).  I added them
into my research workflow.

Recently the models have obtained increasingly new levels of comprehension to
the point where developers are suggesting that models could generate code for a
lay person.

# Rust using AI

# Summary
