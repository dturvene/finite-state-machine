<!-- 
* convert to HTML to view
pandoc medium-article.md -o /tmp/mra.html
google-chrome /tmp/mra.html
paste into medium story

NOTE: Upload to Medium keeps failing.

Title: Converting a C program to Rust Using AI-assisted Development Practices
-->

# Abstract
This article documents my experience converting a
[C] program to [Rust] using AI agentic [prompt engineering].

# Background
On May 11 2021 I published [Implementing a Finite State Machine in C].

The project "demonstrates how to create, manage, drive and regression test a
deterministic finites state machine (DFSM)." It was based on my long-time
professional experience coding FSMs in large scale telecom productss, which I
implemented in [C] and [C++]. 

As my FSM pattern matured I absorbing some traits of the [OMG UML]:

* chapter 14 `State Machines`,
* chapter 16 `Actions`, 
* section 13.3 `Events`.

Many of the concepts in the [OMG UML] document were, for my purposes, 
unnecessary. For example, the OMG State Machine defines a `guard transition`,
and THREE! kinds of states. But the advantage to basing my FSMs on UML is the
nomenclature is well-known, documented, and comprehensive.

My FSMs in the production telecom systems were quite complex with thousands of
instances of different classes running concurrently. A large set of events
were generated for FSM actions, inter-related class instances, hardware events,
and on-demand requests from technicians.
In order to perform unit testing, integration testing and bug triage, this
required me to build a framework to simulate event source/sink in the FSM
instances.  This was, to me, a major benefit of the FSM pattern: triggering
behaviors in simulation.

The production FSMs, while robust, were too complex for demonstration purposes 
so, for [Implementing a Finite State Machine in C], I chose the classic
software pattern for a traffic stoplight FSM and synchronized crosswalk FSM,
including a pedestrian button.
The two FSMs have enough states and events to demonstate the design and
structures but simple enough to understand comprehensively.  Additionally I
added a simple parser for reading a script file to inject events into the FSM
instances for test purposes.

# C to Rust Code Port
Later in 2021 I started to learn [Rust], coding
simple projects to understand the concepts. One goal I had in mind was to port 
my stoplight/crosswalk project to Rust.

However, I stopped my [Rust] research in order to work on Machine Learning (ML)
using [Tensorflow] and [Python].

At the time the [Tensorflow] models were "deep" but not that complex. The
[Tensorflow] tool kit allowed me to create models and train/evalute/test them
on datasets.

Additionally, I accessed pre-trained models including
[Mobilenet v2](https://arxiv.org/abs/1801.04381), 
[Resnet](https://arxiv.org/abs/1512.03385),
[Kaggle Model repo](https://www.kaggle.com/models),
and a huge number more. I would fine-tune the models to better "fit" my
dataset. There were a wealth of datasets to train and evaluate the models, most
notably [US Irvine ML Repo](https://archive.ics.uci.edu/).
As the ML models matured I was fascinated by what they could (and could not)
predict (classification and regression.) 

I started experimenting with [time series sequences] and 
[time series forecasting] models training on historical data from
coinbase for a basket of crypto tokens to predict movement for a target
crypto token.  It was really cool stuff but the models I created and trained
never produced a reliable accuracy level for the evaluation data from the
training data.
I was not confident any would predict future price movements. I invested a good
amount of time in researching and understanding adding, modifying and removing
model layers and how the changes impacted accuracy (sometimes wildly!) but I
did not progress towards increasing the accuracy of the model on the evaluation
data.

# The Rise of LLMs Coding Agents
Around 2023 early [Large Language Model] implementations started to emerge,
including:

* OpenAI [ChatGPT],
* Google [Gemini],
* Anthropic [Claude].

I have not inspected the model layers for any LLM so I am not clear how they
work, but the results were 
[wonderful](https://www.merriam-webster.com/dictionary/wonderful).

I added several LLMs into my software research workflow (beyond google search).
I would ask a question and a detailed explanation would appear. Here are some
representative prompts from my history: 

* `What is the best indian food in arlington virginia?` (I live in Arlington
  but am on a VPN) 
* `Who was Amelia Earhardt`
* `What is the purpose of a python mixin class?`

Recently the models have obtained increasingly new levels of comprehension to
the point where software developers are using models to generate code. One
paradigm is "vibe coding" introduced by 
[Andrej Karpathy](https://en.wikipedia.org/wiki/Andrej_Karpathy). In this
paradigm a subject matter expert (SME) provides requirements to an Agent and it
produces code to fully impelement the requirements. 

"Vibe" has become an often repeated (and misunderstood) buzzword for
[agentic ai system](https://www.ibm.com/think/topics/agentic-ai)
implementations producing code from natural language prompts.
Simon Willison's blog article 
[vibe coding versus ai-assisted programming](https://simonwillison.net/2025/Mar/19/vibe-coding/)
gives clarification to the terms "vibe" and "ai-assisted" coding.

I decided to explore vibe coding and ai-assisted coding on the FSM pattern.

# Rust development using an AI Agent for Ai-assisted coding
I created a `rust_ai` subdir in my [github FSM repo] containing the original
[C] article.  Inside `rust_ai` I experimented with agentic 
[prompt engineering]. I started with vibe coding concepts but it quickly went
off the 
rails.  There is very little that vibe coding achieved for me, other than
"wait, what just happened?" 
However, 
[AI-assisted coding](https://blog.nilenso.com/blog/2025/05/29/ai-assisted-coding/) 
demonstrated huge efficiencies. 

I saw the benefit of ai-assisted coding so in the next step I tested several
popular AI Agents and, almost randomly, chose the free version of [Claude AI].

Next I did on deep dive using [Claude AI] on reconstructing the [C] project in
[Rust]. See the file at
[Rust using AI Agent](https://github.com/dturvene/finite-state-machine/blob/main/rust_ai/README.md)
for a description of my effort.

I started with a simple initial prompt and expanded the prompts and Claude
interaction as I tuned what I **wanted** it to generate.

The third and final prompt experiment (github tag `ai_prompt3`) was a more
comprehensive set of requirements producing a Rust executable more closely
reflecting the original project but it had the most problems.  For example, in
one of the initial prompts [Claude AI] added in a Rust 
[Hashmap](https://doc.rust-lang.org/std/collections/struct.HashMap.html) for
the FSM Transition container.  It would not compile and I had little interest
in coding the necessary functions to support it.

# Epilogue
This project started with a simple effort to code an approximation to
requirements in my 4+ year old project 
[Implementing a Finite State Machine in C] 
using prompt-based agentic technology.

The results from my prompts were great, but not without compile and runtime
issues.  One state transition logic "bug" (crosswalk `BLINKING` event) was
particular hard to track down.

But I am not blaming the "bug" on Claude.  Clearly the
Agent was implementing a requirement I specified, perhaps in not enough detail.
Perhaps the "bug" was in my specification?  Perhaps I assumed too much about
how [Claude AI] plunders [Rust] code fragments?

AI agents can create boiler plate code templates, refactor code, provide
test data and functions, etc. A developer stills needs to provide:

1. a comprehensive set of requirements,
2. possibly tune the requirements for the desired target,
3. build the executable,
4. test the executable against the requirements.

I highly recommend two more of Simon Willison's posts on ai-assisted coding:

* [Agentic Coding: The Future of SW with Agents](https://simonwillison.net/2025/Jun/29/agentic-coding/)
* [Using LLMS for Code](https://simonwillison.net/2025/Mar/11/using-llms-for-code/)

My next steps are parallel efforts to:

* expand my ai-assisted prompts to add clarity and features,
* research [MCP] and [Large Language Model] technology, and APIs to them
  probably using [Python]
* research pros and cons of various agents,
* investigate AI IDEs to improve the development cycle.

As an ancillary effort I want to better learn [Rust] to support my agentic
awareness.  I may also return to [Python] using ai-assisted development.

That is A LOT of research! I will see how it progresses.

# Summary
It is clear to me from my brief deep-dive that AI-assisted software development
is the future.  My analogy is the early days of the automobile.  Animal labor
was still needed (and is still today) but the majority of transportation needs
were assumed by automobiles with increasing functionality and reliability.
Today we are making progress with ai-assisted driving, though full self-driving
("vibe driving" if you will) is still in the distant future.

<!-- external reference links -->
[Rust]: https://www.rust-lang.org/
[Claude AI]: https://claude.ai/new
[Claude Sonnet 4]: https://www.anthropic.com/claude/sonnet
[prompt engineering]: https://docs.anthropic.com/en/docs/build-with-claude/prompt-engineering/overview
[C]: https://www.c-language.org/
[C++]: https://isocpp.org/
[Implementing a Finite State Machine in C]: https://medium.com/@dturvene/implementing-a-finite-state-machine-in-c-aede8951b737
[OMG UML]: https://www.omg.org/spec/UML/2.5.1
[Tensorflow]: https://www.tensorflow.org/
[Python]: https://www.python.org/
[time series sequences]: https://en.wikipedia.org/wiki/Time_series
[time series forecasting]: https://www.tensorflow.org/tutorials/structured_data/time_series
[Large Language Model]: https://en.wikipedia.org/wiki/Large_language_model
[ChatGPT]: https://chatgpt.com/
[Gemini]: https://gemini.google.com/app
[Claude]: https://claude.ai/new
[github FSM repo]: https://github.com/dturvene/finite-state-machine
[MCP]: https://modelcontextprotocol.io/introduction
