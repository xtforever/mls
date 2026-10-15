# You Do Not Know C

### A position paper on the memory-safety debate, the true cost of new languages, and what C was always for

**Author:** [your name]  
**Status:** Draft  
**Date:** 2026

---

## A Note on Authorship and the Use of AI

The ideas, the thesis, and the editorial decisions in this paper are the author's own. Generative AI was used as a research and editing aid: to locate and verify sources, and to help with wording and structure. The core argument, its emphasis, and the choice of what to claim and what to concede are human decisions, and the author takes responsibility for them. The AI is not an author and is not presented as one. Where a verified source contradicts an initial intuition, the paper follows the evidence; an editorial appendix records the seed claims and any deviations.

---

## Abstract

The dominant narrative of the last decade holds that C is fundamentally unsafe and must be replaced by memory-safe languages such as Rust. This paper does not dispute the data on memory-safety bugs. It disputes the conclusion. The failures that make C notorious—out-of-bounds access, use-after-free, unbounded copies—are failures of *how* C is used, not proof that C must be abandoned. New languages are genuine engineering achievements, and there are tasks for which they are the right tool; this paper says so plainly. But the case most often cited for them — a network-facing parser — is the weakest: its memory-safety risk is out-of-bounds access on attacker-controlled lengths, which a bounds-checked C substrate closes, and what remains is input-validation logic, which no language eliminates. But the decision to adopt one is a trade-off, and that trade-off is usually asserted rather than measured. Against the cost of retraining, dual toolchains, FFI boundaries, ecosystem churn, and discarded institutional knowledge stands a *marginal* safety gain that, for many of the same tasks, is small. A Linux daemon such as Redis or memcached is not meaningfully harder to write correctly in C than in Rust, Zig, or Go. C was designed as a portable substrate for building your own abstractions; the correct response to `strcpy` is not a new language but a length-carrying one. This paper presents a concrete existence proof—the `mls` handle library and the toolkit built on it (strings, a sorted dictionary, configuration, HTTP, subprocesses)—showing that most of the genuinely useful features of other languages can be built in C, incrementally, on top of code that already exists, and demonstrates it with small, working tools. The recommendation is not "always C" but "stop choosing a language as an identity and start choosing one as an engineering decision under an explicit threat model."

**Keywords:** C, memory safety, Rust, Zig, Go, systems programming, rewrites, software economics, handles

---

## 1. Introduction: An Intentional Provocation

In 1968, Edsger Dijkstra published a short letter titled *Go To Statement Considered Harmful* [1]. It is remembered as a piece of programming-lore aggression, but it was not a personal attack on programmers who used `goto`. It was an argument: that the unrestricted jump made programs hard to reason about, and that structured alternatives made them easier. The title was a provocation *in order to start a debate*, and the debate changed the practice of programming.

This paper borrows that device. Its title, *You Do Not Know C*, is also a provocation, not an insult. It is aimed at a specific, widespread failure of reasoning: the slide from "memory-safety bugs are common in C code" to "therefore C must be replaced." That slide is one of the most expensive non-sequiturs in modern software engineering. It is expensive because it is used to justify rewrites of working infrastructure on a scale that few organizations can actually afford, often on the strength of a statistic and a sentiment rather than a completed cost-benefit analysis.

The thesis of this paper is short and blunt:

> **C is not the problem. The programmer is—and so is the decision to treat a language choice as a moral identity rather than an engineering trade-off.**

> **Memory safety can be achieved in C.** C the *language* is not memory-safe; a C *program* can be—through the abstractions and discipline the language exists to support. The real choice is therefore not "memory-safe language vs. unsafe C," but "which costs you pay for safety, and where."

To be clear about what this does *not* mean:

- It does not mean C is memory-safe. It is not. This paper takes the memory-safety data seriously.
- It does not mean you should never use Rust, Zig, or Go. There are real cases where you should.
- It does not mean "real programmers write everything by hand." The opposite: the argument *depends* on building better abstractions.

What it does mean is that the industry has, in the space of a few years, converted a genuine engineering trade-off into a matter of faith. This paper tries to put the trade-off back where it belongs: on a balance sheet, with numbers on both sides.

Section 2 states the case against C as strongly as the evidence allows. Section 3 presents the new-language answer fairly, including the strongest counterargument to this paper's thesis. Section 4 develops the thesis. Section 5 tests it on concrete case studies. Section 6 argues that C was always meant for building your own language, and demonstrates it. Section 7 turns the argument into recommendations. Section 8 concludes and invites disagreement.

---

## 2. State of the Art: The Case Against C, Stated Strongly

A position paper that strawmans its opponents is worthless. This section therefore presents the case against C in its strongest form. Everything here is real, and most of it is damning.

### 2.1 The memory-safety statistics

The central empirical claim of the "replace C" movement is that memory-safety defects dominate serious vulnerabilities. The numbers are striking, and they should be quoted with their scope rather than as a universal constant.

- **Microsoft (2019).** In a widely cited MSRC analysis, roughly **70% of the vulnerabilities Microsoft assigns a CVE to each year are memory-safety issues** [2]. The underlying analysis was presented by Matt Miller at BlueHat IL 2019 [3]. This is a statement about Microsoft's own products over time, not about all software.
- **Chromium (2020).** The Chromium project reports that around **70% of its serious security bugs are memory-safety problems**, based on an analysis of **912 high- or critical-severity security bugs since 2015**; about half of those are use-after-free [4]. This is scoped to a single (enormous) C++ codebase.
- **Android (2019).** Google estimated that around **90% of Android vulnerabilities are memory-safety issues** [5].
- **Apple (2019).** One analysis found that **60–70% of vulnerabilities in iOS and macOS are memory-safety issues** [6].
- **Exploited zero-days.** An analysis of zero-days exploited in the wild found that **more than 80% were memory-safety vulnerabilities** [7].

The pattern is consistent: wherever large C or C++ codebases are measured, memory-safety defects account for the majority of serious security bugs. Any serious treatment of the topic must begin by conceding this.

### 2.2 The landmark failures

Statistics become vivid in specific failures. Several of the most consequential security events of the last two decades were memory-safety bugs in C or C++.

- **Heartbleed (CVE-2014-0160, 2014).** An out-of-bounds read in OpenSSL's heartbeat handling leaked private keys and session data from a large fraction of the secure web [8]. The root cause was a missing bounds check on a length field—a buffer over-read.
- **EternalBlue (CVE-2017-0144, 2017).** A use-after-free-class vulnerability in Microsoft's SMB implementation, weaponized by the NSA and leaked; it powered the WannaCry ransomware, which disrupted hospitals and other critical services [9].
- **SQL Slammer (2003).** A buffer overflow in Microsoft SQL Server became one of the fastest-spreading worms in history, saturating networks within minutes [10].

These are not obscure corner cases. They are the canonical examples taught in every security curriculum, and they are all memory-safety failures in C and C++.

### 2.3 The institutional response

The response has not been confined to engineering blogs. Institutions with substantial authority have taken positions.

- The **NSA** published a Cybersecurity Information Sheet, *Software Memory Safety* (November 2022), recommending that organizations use memory-safe languages where possible [11].
- **CISA** and international partners published *The Case for Memory Safe Roadmaps* (December 2023), urging vendors to plan the transition of critical software to memory-safe languages [12].
- The U.S. **Office of the National Cyber Director (ONCD)** published *Back to the Building Blocks: A Path Toward Secure and Measurable Software* (February 2024), which explicitly frames memory-safe languages as a strategic national priority [13].

When the NSA, CISA, and the White House all point the same direction, "keep writing C the way you always have" stops being a defensible default.

### 2.4 The rewrite movement

A concrete migration is underway, not just a debate.

- **Prossimo / ISRG** funds memory-safe rewrites of critical Internet infrastructure, including `sudo`/`su`, the TLS library Rustls, `ntpd-rs`, and decoders [14].
- **sudo-rs**, a Rust reimplementation of `sudo` and `su`, reached a first stable release in August 2023, moved to the Trifecta Tech Foundation, and was announced as the default `sudo` for Ubuntu [15]. Prossimo reports reducing its dependency count from 135 to 3 [14].
- **Chrome and Android** now accept—and in places require—Rust for new network-facing components [4].
- **Rust for Linux** has brought a second language into the kernel, with initial support merged in 2022 and a growing set of drivers and abstractions [16].

### 2.5 What the data does—and does not—establish

Before drawing conclusions from these numbers, it is worth being precise about what they measure. Four caveats matter.

First, the statistics are **defect counts within particular codebases**, not defect rates per unit of engineering effort. "70% of CVEs" says nothing about how many bugs a careful team would produce in the same time in another language; it says that, among the bugs that reached CVE severity, most involved memory. It is a statement about a *mix*, not a *rate*.

Second, the samples are **not representative**. Chromium, Windows, Android, and iOS are enormous, adversarial, high-value targets, written by many thousands of contributors over decades, with attackers actively hunting them. They are the hardest case, not the typical case. Extrapolating from the most attacked software in the world to a company's internal file-processing tool is a category error.

Third, the data does not **control for process**. The same organizations that report high memory-safety rates also deploy the most advanced fuzzing, sanitizers, and review—and still find bugs. That is used as proof that tools are "insufficient." But it also shows the difficulty is intrinsic to the *task*—hostile, complex, fast-moving input parsing—not merely to the syntax of the language.

Fourth, language migration does not eliminate defects; it **redistributes** them. Newly written code has its own defect curve, and the bug classes that replace memory bugs—logic errors, `unsafe`/FFI mistakes, asynchronous and ownership mistakes—are real. A rewrite is not a subtraction of bugs; it is a replacement of one bug distribution with another, plus the regressions introduced while writing it.

None of this refutes Sections 2.1–2.4. The case against C is real. The point is that the numbers establish "memory-safety bugs dominate serious vulnerabilities in large, hostile-target C/C++ codebases." They do not, by themselves, establish "replace C wherever it is used." That is a further inference, and it needs its own evidence.

### 2.6 The philosophical claim

Underlying all of this is a philosophical claim, and it is the claim this paper challenges:

> Memory-safety bugs are a *root cause* of insecurity, and the root cause of the root cause is the language. Therefore the correct fix is to change the language.

This claim is worth taking seriously, because it is *partly* true. A language that makes a class of bugs unrepresentable is a genuine advance—not a cosmetic one. Rust's ownership and borrowing rules really do prevent use-after-free and data races at compile time, without a garbage collector [17]. That is a real and important achievement.

The error is not in the premise. It is in the conclusion, and especially in the strength of the conclusion. "A language can eliminate a class of bugs" does not imply "the language is the only, or the best, response to that class," nor "the cost of switching is justified wherever the bugs appear." The next section takes the new languages seriously before arguing about the price.

---

## 3. The New-Language Answer—and the Shape of the Question

### 3.1 What Rust, Zig, and Go actually offer

It is worth being precise about what each language brings, because "memory safe language" is not one thing.

**Rust** is built around ownership, borrowing, and lifetimes. Its signature guarantee is that, in safe code, there is no use-after-free, no double-free, and no data race, and this is enforced at compile time without a tracing garbage collector [17]. The cost is a type system and learning curve that many experienced systems programmers describe as substantial, plus an `unsafe` escape hatch that must be used for FFI and low-level work—which is, notably, precisely the work systems programmers do. Rust's tooling (`cargo`), package ecosystem, and testing story are genuinely excellent.

**Zig** takes a different path. Its pitch is "no hidden control flow, no hidden allocations": no operator overloading, no exceptions, no invisible heap use; allocators are passed explicitly; `comptime` replaces macros; the language can compile C and exposes the C ABI directly [18]. Zig's designers explicitly position it as a "portable language for libraries" and as a *toolchain* that can replace CMake/Make for existing C projects [18]. Its safety features (checked arithmetic, debug allocators that detect use-after-free, leak detection) are strong in debug builds and opt-in in release.

**Go** offers garbage collection, fast compilation, simple concurrency (goroutines/channels), and a very approachable language surface [19]. It is an excellent choice for network servers and tooling. Its trade-offs are the familiar ones of garbage-collected languages: runtime and memory overhead, GC pauses (small, but nonzero), and hidden allocations that make it a poor fit for the strictest real-time or bare-metal contexts [18].

All three are serious languages with real benefits. The rest of this paper accepts that.

### 3.2 The cost side of the ledger

The benefits above are real, and so are the costs that accompany them. They are rarely listed together.

- **Learning curve.** Rust's ownership and borrow-checking model is the source of its safety and the main source of its difficulty. Teams report a substantial ramp-up, and the compiler is strict by design [17]. This is a rational trade, but it is a cost, and it is paid in engineering time that the project does not get back.
- **The `unsafe` edge.** Rust's guarantees hold in safe code; FFI, low-level data structures, allocators, and some concurrency work require `unsafe`, where the compiler's protection stops. Systems programming lives disproportionately close to that edge—which is precisely where the C pitfalls return.
- **Maturity.** Zig states outright that it is a simpler alternative to C++, D, and Rust, and frames itself as a toolchain and library substrate [18]; it is also still stabilizing, which is a real adoption risk for infrastructure meant to last decades. That is not a criticism of Zig's design, which is thoughtful; it is a statement about where a long-lived project can responsibly place its dependencies today.
- **Runtime and footprint.** Go's simplicity and concurrency come with a runtime and a garbage collector [16],[18]. For servers that is often a fine trade; for kernels, bootloaders, and bare metal it is disqualifying.
- **Ecosystem surface.** Every dependency in a young ecosystem is code you did not write, may not audit, and must trust. Supply-chain risk is not removed by the language; it is moved to a different set of maintainers.
- **Toolchain and process.** Two languages mean two build systems, two package managers, two testing and observability stories, and a boundary between them [7].

None of this is an argument for C by default. It is an argument for *seeing both columns* before choosing.

### 3.3 The strongest counterargument

The best statement of the opposing case is not a blog rant; it is the careful version published by Prossimo (ISRG). Their survey of memory safety asks the right question directly:

> "Are there techniques we can undertake to reduce this risk, without forcing ourselves to entirely change programming languages? And the answer is absolutely yes. Not all projects written in unsafe languages are equally unsafe and unreliable." [7]

They then list what those techniques are: modern idioms, fuzzers, sanitizers, exploit mitigations, and privilege separation. Their argument is that these are "woefully insufficient," because the very teams with the most investment in them—browser and OS developers—still produce an avalanche of memory-safety bugs.

This is the strongest form of the case, and this paper will not pretend it is weak. Two responses are necessary.

First, "insufficient" is a claim about a *threat model*, and threat models differ. For a browser parsing hostile input from the entire Internet, with a dedicated security team and a decade-long adversarial target painted on it, the residual memory-safety rate is a strategic liability. For a local build tool, a batch data processor, an embedded controller on a closed network, or a daemon behind a firewall, the same residual rate is a background risk that the organization may rationally decide to manage rather than eliminate by rewriting. Prossimo's own framework is more nuanced than the slogan: they recommend starting with *new* projects and *new components*, and choosing a rewrite only where a rewrite was already being contemplated [7]. That is a far cry from "rewrite everything."

Second, the counterargument measures the benefit of memory safety but not its *price*, and it does not measure the price of the alternative at all. The same Prossimo survey is admirably candid about the price:

> "Changing the programming language your organization uses is not something to be undertaken lightly. It means changing the skills you're looking for when you hire, it means retraining your workforce, it means rewriting large amounts of code." [7]

That is the trade-off this paper is about. The memory-safety movement has produced excellent data on one side of the ledger and almost none on the other. The rest of this paper is an attempt to write down the other side.

### 3.4 The question, restated

The useful question is therefore not "is C safe?" (no) or "is Rust better?" (for some things, yes). It is:

> For a given component, under a given threat model, does the *marginal* reduction in risk from a new language exceed the *total* cost of adopting and living with it—including the cost of discarding working, tested, understood infrastructure?

Answer that question component by component, and the debate becomes engineering. Refuse to ask it, and the debate becomes religion. Section 4 takes up the thesis; Section 5 works through concrete instances.

---

## 4. The Thesis: C Is Not the Problem

### 4.1 Distinguishing the mechanism from the cause

Consider the use-after-free. It is a *mechanism*: memory is released, then accessed. The *cause* is a program whose logic frees a resource and then uses it—a logic error. A memory-safe language removes the ability to express that error. That is a real benefit. But note what it is not: it is not a statement that the C programmer's understanding was absent, nor that the task was beyond C. It is a statement that one class of mistake is now impossible to *express*. Eliminating a failure mode is valuable; it is not the same as being the only way to reach the goal.

This distinction matters because the "root cause" language smuggles in a stronger claim. If the language were the root cause, then any C program would be doomed in proportion to its size, and any rewrite would necessarily be safer. Neither holds. Large, careful C programs—the ones this paper discusses in Section 5—have excellent safety records, and rewrites have their own well-documented failure modes. The root cause of a specific bug is a specific decision; the language determines which decisions are *representable*, not which are *made*.

### 4.2 Complexity is conserved, not destroyed

The second part of the thesis is that complexity is largely conserved. A new language does not delete the difficulty of systems programming; it relocates it. Rust buys compile-time memory safety at the cost of a type system that many teams find harder to learn than C [17], an `unsafe` subsystem where the guarantees do not hold, and a build/FFI boundary that becomes a new surface for bugs. Zig, by its own account, responds to the "distracting" complexity of C++ and Rust with a simpler language [18]—an admission that the complexity of the earlier solutions had itself become a problem. Go buys simplicity with a runtime and a garbage collector [19], [18], which is exactly the kind of hidden cost that matters in a kernel or an embedded context.

None of this is an argument against those languages. It is an argument against the assumption that they place the system beyond error. They change the error *distribution*. A team that was finding use-after-frees may now find borrow-checker fights, `unsafe` block audits, async lifetime puzzles, or FFI-boundary ownership bugs. Whether the new distribution is better *for that team, on that component* is an empirical question. It is rarely asked.

This friction is not a one-time learning cost; it is a standing tax on exploration. To write the first version of an idea you must already have decided who owns what and how long each reference lives — decisions that, in an unsafe language, are normally made *after* the idea works, once its shape has settled. The borrow checker refuses to run code whose lifetimes it cannot prove, so the exploratory loop of sketch, run, reshape becomes a negotiation with a compiler exactly at the moment the shape is least settled. The cost is paid most by the people doing new work, not by those porting a finished design.

### 4.3 The compounding value of decades

The most under-weighted term in the entire debate is the value of what already exists.

C has been in continuous use since 1972 [20]. The accumulated body of C and C-compatible infrastructure is not a liability to be written off; it is one of the most valuable artifacts in computing. Consider a partial list of what runs on it:

- **Linux**, the kernel underneath most servers, all Android devices, and most of the cloud [21].
- **SQLite**, almost certainly the most widely deployed database engine in the world, embedded in phones, browsers, and operating systems, with an extraordinary test suite [22].
- **PostgreSQL**, **Redis**, **memcached**, **nginx**, **curl**, **OpenSSL**, and **Git**—each a foundational, battle-tested, widely audited system [23], [24].
- **DOOM**, which shipped on millions of systems and was ported to essentially every platform that exists [25].

The relevant word is *tested*. These systems have absorbed billions of hours of operational use and adversarial attention. That testing is a form of assurance that cannot be transferred to a rewrite for free. A rewrite starts with zero operational history and a fresh supply of undiscovered bugs; the old code has already paid the bug tax. This is why rewrites so often trade a known, bounded risk for an unknown, unbounded one.

The other half of the decades is the one the rewrite ledger never writes down: buildability in the future. C's value is not only that its code has been *tested* for fifty years but that it can plausibly still *build* in forty more, as it already has. A C program is a compiler plus a handful of old, stable dependencies; version churn at the language level has been modest and backward-compatible in practice (C89 code still compiles), and the C ABI is the one stable point every other language targets for interop (§4.5). Rust, by contrast, deliberately offers no stable ABI: only the `extern "C"`/`#[repr(C)]` boundary—C's ABI—is stable, so a Rust-native-linked component must be rebuilt from source against a dependency graph that must still resolve and compile decades from now; Zig has not reached 1.0. This is not a claim that new languages will disappear; it is a cost term—churn and longevity risk—that the migration ledger omits.

The whole point of C is that there is no runtime to port. A C program is the source, the compiler, and the platform ABI — and Linux userspace famously *never breaks*, a promise the kernel has kept for decades. A K&R-era program can be brought to a bleeding-edge system absolutely and without doubt: recompile it, and rework the small part that no longer compiles. That rework is bounded precisely because the language is small enough to hold in your head — the whole point of C. Even the worst case, rebuilding against old and rotten libraries, reintroduces their old bugs but still produces a program that runs. Compare: bringing an old Go program forward means rebuilding with a version-locked toolchain and a runtime the size of a language, and that toolchain must first exist for the target. C asks you to port a program; the new languages ask you to port a runtime and a toolchain. That asymmetry is the longevity argument, and it is the point of the language, not a side effect of it. (The one binary-level caveat — a binary linked against a newer glibc may not run on an older one — is a property of how you distribute, not of the language: ship source or link statically, and it disappears.)

### 4.4 The trade-off, written down

Putting the terms together, the decision to move a component to a new language has, at minimum, these costs:

1. **Rewrite cost.** Reimplementing the component, plus the regression risk of newly written code.
2. **Loss of institutional assurance.** The devaluation of years of testing, auditing, and operational hardening.
3. **Retraining and hiring.** The workforce now needs a second skill set; the labor market is thinner for the new language [7].
4. **Dual toolchains.** Build systems, package managers, CI, crash reporting, and deployment must support two ecosystems [7].
5. **FFI and boundary risk.** Every call across the language boundary is an ownership/ABI hazard, often in `unsafe` code—the very category of risk the change was meant to remove.
6. **Ecosystem maturity risk.** Dependencies in a young ecosystem may be immature, unmaintained, or themselves unsafe.
7. **Opportunity cost.** Engineering effort spent rewriting is effort not spent on features, performance, or hardening the existing system.
8. **Churn and longevity risk** (the future twin of item 6). Even a mature dependency must still exist and compile in forty years. Rust's crate graph must still resolve; Zig is pre-1.0. C's ABI stability means the old code does not carry this liability—with the same caveat as §4.3: the platform (libc) is less stable than the language, so the relevant case is the statically-linked or freestanding program.

Against this, the benefit is a reduction in a specific class of defects. The thesis is not that this benefit is zero—it is not. The thesis is that for many of the *same tasks*, the benefit is small relative to the cost, and the field's default recommendation has stopped doing the arithmetic.

### 4.5 C as the interoperability substrate

There is a deeper reason the value of C is under-counted: C is not merely one language among many. It is the *substrate* the others stand on.

Every memory-safe systems language today interoperates through the C ABI. Rust exposes an `extern "C"` boundary for FFI and is being integrated into a C kernel [16]; Zig's own pitch is that it can conform to the C ABI and compile C, explicitly aiming to be "the new portable language for libraries" *because C is currently that language* [18]. Much of the toolchain compiles through LLVM, itself written in C++. The operating systems, libraries, and drivers that a new language wants to call were, almost without exception, written in C. When a project "moves to Rust," it does not leave the C world; it enters into a long-term relationship with it at every boundary.

This matters for the cost ledger. Replacing a component is not like replacing a library; the substrate is not optional. The more of a system that moves to a new language, the more *boundary* there is, and the boundary is exactly where ownership and `unsafe` hazards concentrate. C's role as the universal interface—unglamorous, occasionally unpleasant—is a genuine public good, and it is not reproducible by fiat.

### 4.6 Disclosure

The author of this paper is the author of `mls`, the C library that appears throughout it—as the memory-safety existence proof of Section 6, as the toolkit behind the small-tool case study of Section 5.5, and as a recommended abstraction in Section 7. That is a conflict of interest, and because `mls` recurs rather than appears once, it is disclosed here rather than buried. Two honesty constraints follow. First, the argument does not depend on `mls` being unique or optimal; the principle—build the abstraction you need—has many implementations, and `mls` is one. Second, the demonstration programs cited for `mls` (Section 5.5) are its own examples, from this repository; the paper does not claim third-party adoption it cannot evidence. Readers should weight the claims about `mls` accordingly, and should expect the paper to be judged on its reasoning and its citations, not on the library.

---

## 5. Case Studies: The Same Task in Different Languages

Abstract arguments about "complexity" are weak. This section fixes a comparison basis and applies it.

### 5.0 Comparison basis and threat model

For each case, we compare along named axes rather than by impression:

- **Functional equivalence.** Are the two implementations doing the same job, with the same features?
- **Size.** Rough lines of code (or bytes of source) for equivalent functionality.
- **Dependency and FFI surface.** How many external components, and how much code crosses a language boundary?
- **Raw/unsafe lines.** How much code operates outside the language's safety guarantees?
- **Operational record.** Known memory-safety CVEs in the deployed artifact.
- **People cost.** Maintainers, reviewers, and required expertise.
- **Threat model.** Privileged vs. unprivileged; network-facing vs. local; parsing hostile input vs. trusted input.

The threat model is stated explicitly because the "is it safe enough?" question is meaningless without one. A component that parses attacker-controlled input across a privilege boundary has a very different risk profile from one that transforms local files owned by the user.

### 5.1 Daemons: Redis and memcached

Redis and memcached are network daemons written in C. They are among the most widely deployed and most reliable pieces of network software in existence [23], [24]. Both are small enough to be read, and both have survived decades of adversarial deployment. Their reliability is not an accident of the language; it is the product of careful design, small attack surface, and sustained maintenance.

Now consider writing a comparable daemon from scratch. In C, a minimal event loop with a binary protocol and a hash table is a few hundred to a few thousand lines, built directly on POSIX. In Rust, the same functionality typically pulls in an async runtime, a protocol/serialization stack, and a set of ecosystem dependencies; the code may be comparable in length, but the dependency graph and the total code that must be trusted are larger. In Go, the code is very short, but the binary carries a runtime and a garbage collector, and the memory profile is correspondingly larger [19], [18]. Zig is the closest in spirit to C—explicit allocators, C ABI, tiny runtime—and its own documentation frames it as a better *toolchain and library substrate* more than as a smaller way to write a daemon [18].

The honest conclusion for this task is not "C is easier." It is that **the difficulty of writing a correct event-driven daemon is dominated by the design and the protocol, not by the language**, and that C is not meaningfully harder here. The memory-safety risk is real but bounded for a small, carefully reviewed codebase behind a network boundary. The claim "you must use a new language to write a daemon" is not supported by the existence of the best daemons in the world, which are written in C.

### 5.2 Privileged tools: sudo versus sudo-rs

`sudo` is the canonical C utility at a privilege boundary. It is ancient, heavily audited, and has a long history of occasional memory-safety flaws—which is exactly why Prossimo chose it for a rewrite [14]. `sudo-rs` is a genuine achievement: a memory-safe implementation, with a drastically reduced feature set and a dependency footprint reduced from 135 to 3 [14], and it is becoming the default `sudo` in Ubuntu [15].

This case should be granted. It is the strongest argument for the rewrite position, for three reasons:

1. The component sits at a privilege boundary, where the threat model is maximally hostile.
2. The rewrite *reduced* attack surface by dropping rarely used features—a security win partly independent of the language.
3. A trusted maintainer of the original advised the rewrite team [14], mitigating the "discard institutional knowledge" risk.

But even here, the correct reading is not "C failed." It is that a specific privileged component, with a specific threat model, benefited from a carefully scoped rewrite that also reduced features and dependencies. The lesson is about *scope and threat model*, not a blanket verdict on C. And note the cost: years of funded effort, a dedicated foundation, and an organizational commitment that most software teams cannot replicate. The same decision applied indiscriminately to the long tail of C software would be catastrophic for the industry's ability to ship anything at all.

### 5.3 Existence proofs at scale: Linux and DOOM

Two data points cut directly against the claim that C cannot support the tasks it is used for.

**Linux** is a C kernel of tens of millions of lines that runs most of the Internet, all Android devices, and the majority of the cloud [21]. It has, over three decades, developed an institutional process—review, testing, fuzzing, static analysis, and hardening—that manages memory-safety risk at a scale no other project matches. Rust is now being added as a second language [16], and that is a reasonable evolution. But the existence of a working, secure-enough C kernel at that scale is a demonstration that C is *sufficient* for the same class of tasks, and that process and discipline are load-bearing variables.

**DOOM** is a C program that shipped on millions of systems and was ported to essentially every computing platform, from calculators to embedded devices to modern consoles [25]. It is a game engine—the very "systems programming" task that is supposedly infeasible in C. Its longevity and portability are a direct product of C's simplicity and ABI stability.

The point of these examples is not "therefore never use Rust." It is that the assertion "you cannot do X safely in C" is empirically false for the broad class of X that C has actually been doing for decades. If the claim is instead "you cannot do X *as safely* in C," that is a different, measurable, and much narrower claim—and one that must be weighed against the costs in Section 4.4.

### 5.4 Where Rust genuinely wins

Intellectual honesty requires listing the cases where the rewrite position is clearly right:

- **New, network-facing components that parse hostile input.** A new TLS implementation, a new DNS resolver, a new image or media decoder, a new browser subsystem. Here the threat model is hostile, the code is new (so no legacy assurance is lost), and memory safety is directly load-bearing. Rust's guarantees pay for themselves.
- **Security-critical, privilege-boundary code that is already being rewritten.** `sudo-rs` is the model: rewrite when you were going to rewrite anyway, and reduce features while you are at it.
- **Concurrent code where data races are the dominant risk.** Rust's `Send`/`Sync` and borrow rules are a real advance for shared-memory concurrency [17].
- **Greenfield codebases with no legacy C to interoperate with.** The cost of a new language is lowest when there is no boundary to cross.

The thesis of this paper is fully compatible with all of these. The disagreement is with the *generalization*—with the leap from "these are good uses of Rust" to "C should be replaced."

### 5.5 Small tools and monitoring daemons: `mls` in practice

The case studies above are large, old, famous systems. The harder question for a working engineer is smaller and more immediate: *can an ordinary team write an ordinary tool in C today without paying a productivity penalty?* The `mls` examples in this repository are the paper's concrete answer, and they are reproducible [27].

Several are demonstration programs built directly on `mls`—small enough to read in one sitting, real enough to build and run:

- **`procwatch`** monitors process creation through `/proc/<pid>/status`, reporting each new process with its identity (`pid`, `uid`, `gid`, `ppid`, `comm`), with a `learn`/`exec` model and clean signal handling (SIGINT/SIGTERM/SIGUSR1 stop it, SIGHUP reloads its allowlist). It is a small daemon that a team would otherwise write as a shell or Python script.
- **`netwatch`** monitors TCP endpoints through `/proc/net/tcp`, learning `LISTEN` sockets (a backdoor or malicious systemd unit) and `ESTABLISHED` targets (C2 or exfiltration), and resolving each new listener to its owning process through `/proc/<pid>/fd`. It is a network-security sensor written in C.
- **`csv2insert`** converts CSV to SQL `INSERT` statements—a demonstration of the `m_var` expansion system, with a fixed column list and table name and a naive comma split, not a production ETL tool; **`hdf_dump`** prints an HDF config tree; **`world`** walks a simulated-world handle graph and emits Graphviz DOT, rendered to SVG or PNG. None of these is systems programming in the narrow kernel sense; they are exercises in the toolkit.

What makes them relevant is not their size but their *cost profile*. They are built on the same "Python-comfort" primitives that make high-level languages pleasant—`m_dir_list` instead of `opendir`, `m_str_from_file` plus `s_trim` instead of manual `fopen`/`fgets`, `m_lookup` as a sorted set with O(log n) membership, `m_qsort`, and `m_strlist_save`/`load` for persistence—while retaining C's deterministic release, small binaries, and freedom from a runtime or garbage collector. The claim is not that these tools are shorter than their Python equivalents; it is that the *engineering* is comparable, and the result is a self-contained binary with no interpreter and no GC pauses.

This is the small-scale twin of Section 5.1. There the point was that Redis and memcached are not meaningfully harder in C than in Rust, Zig, or Go; here the point is that a hundred-line monitoring daemon is not meaningfully harder either, once a length-carrying, bounds-checked substrate is in place. The examples live in the repository; they build and run, and they are the paper's own.

### 5.6 The daemon that ran out of memory

The demonstrations above are small. The production case is where the argument bites. Consider the simplest service in a real infrastructure: it receives an HTTP message from the internal network, runs a few database queries, stores a row in MariaDB, and reports back. In Flask this is ten minutes of work—and it runs fine until there are a few hundred of them. A few hundred to a thousand concurrent Python runtimes, each a full interpreter with its modules loaded, exhaust a production server's memory and the OOM killer reaps the fleet. The task is trivial; the runtime is what does not scale.

The same service, written once against `mls`, is a small C daemon. The SQL is generated by the `m_var` string expansion that drives `csv2insert` (`vs_sql_string_builder` quotes and expands a template); `m_http` and `m_flask` supply the parser and routing that Flask and gunicorn otherwise provide. The first daemon costs more than the Flask version—there is no pretending otherwise—but it is the only one that costs anything: the second and the hundredth reuse the abstraction and cost almost nothing. The advantages are not marginal. No interpreter, no garbage collector, no runtime to port; one process serving thousands of connections over `epoll`/`poll` instead of a process per connection; a memory footprint in kilobytes where Python spent tens of megabytes per worker; and the process model the operating system actually exposes—`fork`, signals, `poll`, `epoll`—rather than an abstraction layered on top of it. This is not a promise about what could be built; it is what the Section 6.4 toolkit is for, and the first daemon pays for the rest.

---

## 6. The Real Solution: C Was Always Meant for Building Your Own Language

The claim of this section is blunt and positive: **memory safety is achievable in C.** A memory-safe language enforces bounds and lifetimes inside an abstraction it provides; in C you build that abstraction yourself, once, and enforce the same rules. The difference is not capability, but who pays for the abstraction.

### 6.1 C as a substrate, not a product

Dennis Ritchie described C as a systems implementation language: minimal, close to the machine, and portable, designed so that the operating system and its tools could be written in it [20]. It was never meant to be a complete application framework. It is a *substrate*—a portable assembler with enough structure to build abstractions on top of. The C standard library is deliberately small; it is not the point of C.

This is the crux of the misunderstanding in the title. Programmers who say "C is bad" often mean "raw libc string handling is bad." They are right about the latter and wrong about the former—because the correct use of C is to *build the string handling you need*, not to use `strcpy` and hope.

### 6.2 The `strcpy`-without-`n` mistake

The canonical complaint is the unbounded copy. A programmer writes `strcpy(dst, src)`, the source is longer than the destination, and the program corrupts memory. The complaint is usually framed as "I need `strncpy` instead."

This framing is itself a trap, and it is worth being precise:

- `strcpy` is unbounded; it copies until the source's null terminator.
- `strncpy` is *not* a clean fix. It copies at most `n` bytes but does **not** guarantee a null terminator when the source is too long, and it zero-fills the remainder when the source is shorter—a silent performance cost and a classic source of its own bugs (CWE-170 and related).
- The *real* fix is a **length-carrying abstraction**: an API that knows how large a buffer is, checks before it writes, resizes when needed, and reports failure instead of corrupting memory.

This has been understood for decades. OpenBSD introduced `strlcpy`/`strlcat` in 1998 specifically because `strncpy` was error-prone [26]. Safe string libraries, dynamic buffers, and arena/handle systems are standard practice in serious C code. The industry did not fail to notice the problem. It failed to *standardize* the solution into libc, and then some programmers concluded that the language was at fault.

The correct conclusion is not "abandon C." It is "put the length in the abstraction"—which is exactly what every serious C codebase does, and what every memory-safe language does internally.

### 6.3 Existence proof: the `mls` handle library

To show that the genuinely useful features of other languages can be built *in C*, consider `mls`, a C11 library that replaces raw pointers with integer handles. It is not the only such library, and it does not solve everything, but it is a concrete, working demonstration of the principle.

The core idea: a handle is a small positive integer. The actual buffer and its length live in a library-managed table. A handle is stable across `realloc`; the caller never holds a pointer into library memory across a resize.

```c
m_init();

int h = m_alloc(10, sizeof(int), MFREE);   // dynamic int array
for (int i = 0; i < 5; i++)
    m_put(h, &i);                          // bounds-checked, auto-resizing

int x = INT(h, 2);                         // checked element access
m_free(h);                                 // recursive teardown
m_destruct();
```

Against the failure classes from Section 2, the library provides:

1. **Bounds checking and auto-resizing.** `m_put` and `m_write` check and grow the buffer; indexed access through the typed macros (`INT`, `CHAR`, `DOUBLE`, ...) is bounds-checked, and the `_SAFE` variants return an error instead of aborting. An out-of-range access on the normal API is a loud error, not silent corruption. This removes the Heartbleed-class out-of-bounds read/write *pattern* from code that goes through the API rather than raw pointers.

2. **Use-after-free becomes a deterministic, loud error.** Every handle encodes a 7-bit use-after-free protection tag (bits 24–30; `REAL_UAF(m) = (m >> 24) & 0x7f`). Each list stores the tag it was created with, and the global tag is advanced on every free (`UAF_PROTECTION = (UAF_PROTECTION + 1) & 0x7f`). On access, the handle's tag is compared against the slot's; a stale handle no longer matches and the library aborts with a clear error instead of reading freed memory. The ceiling is honest: the 7-bit tag wraps after 128 frees, at which point a stale handle could match again. The point is not that this is cryptographically unbreakable; it is that the *common* UAF—use immediately after free—is turned into an immediate, high-signal crash, which is exactly what a developer wants during development.

3. **Deterministic recursive teardown.** `m_free` runs a registered free handler; `MFREE_EACH` frees child handles, and `MFREE_STR` frees string elements. `m_free` marks a handle as being freed *before* running the handler, and skips already-freed children, so a graph with back-references (including a cycle) tears down without infinite recursion or double-free. This is manual memory management with the ergonomics of a destructor.

4. **RAII-like scoping.** `tmls` is a handle type carrying GCC/Clang's `__attribute__((cleanup))`, so a handle is freed at block exit on compilers that support it [27]. This brings scope-based resource release—one of the features people most associate with C++ and Rust—into portable C (with a documented fallback to plain `int` on MSVC).

5. **Leak detection in release builds.** `m_count_allocated()`, `m_total_bytes()`, and `m_peak_handles()` expose live-handle counts without AddressSanitizer. A steadily rising handle count is a visible leak signal in an ordinary build, and `mls`'s own examples bracket their work with `m_count_allocated()` asserts to prove they return to baseline.

6. **One container for many shapes.** Because every list is an `int` handle, the same primitives (`m_put`, `mls`, `m_len`, `m_free`) build arrays, lists, trees, graphs, and strings. This is the "one API for everything" property that proponents of high-level languages cite as a productivity advantage—implemented in C, with bounds checks.

The library is not a panacea, and honesty requires naming its ceilings: it adds a lookup indirection (a performance cost), it caps concurrent distinct handles (24-bit index), its UAF tag wraps after 128 frees, and it does not, by itself, prevent every possible concurrency race. It is an existence proof of a method, not a universal solution. But that is the whole point: the method—*build the abstraction you need*—is available in C, and always has been.

### 6.4 Beyond memory safety: the `mls` toolkit

Memory safety is only half of what the `mls` example is meant to demonstrate. The other half is this paper's abstract claim—that *most of the genuinely useful features of other languages can be built in C*—and it deserves concrete form rather than assertion. `mls` is not only a handle allocator; it is a small library of the same primitives that make high-level languages productive:

- **Strings (`s_*`).** A real string type: `s_new`, `s_printf`, `s_slice` (null-terminated), `s_split`/`s_join`, `s_find`, `s_replace_c`, `s_trim`, case conversion and comparison, number formatting and parsing, base64, and a constant-time comparison for secrets—the operations a Python or Go programmer reaches for by default.
- **A sorted dictionary (`tbl_*`).** `tbl_set`/`tbl_get`/`tbl_del` over string keys with O(log n) lookup, values as strings or owned handles—the role Python's `dict` plays, without a fresh hash-table implementation in every project.
- **Lists and trees (`list_*`).** A universal node structure whose children are handles, used to build JSON-like trees; it is the basis of the HDF reader and the table.
- **Configuration (`m_hdf`).** An S-expression config format with both a parser and a writer—what most projects otherwise pull in TOML, YAML, or JSON for—and the same format drives `m_flask` routing.
- **HTTP (`m_http`, `m_http_server`, `m_flask`).** A client, an embedded server, and routing driven by an HDF config: a minimal Flask for C daemons and tools.
- **Subprocesses (`m_subproc`).** `subproc_run`/`subproc_read`/`subproc_lines`, the `subprocess` module in miniature.
- **Drop-in ergonomics (the amalgam).** `generate_amalgam.py` produces single-file headers (`mls_core.h`, `mls_full.h`) that compile with one `#define` and one `#include`, in the spirit of the `stb` libraries; the core is thread-safe per handle, and `_SAFE` variants plus `mls_errno` give handleable errors where the normal API aborts loudly.

None of these is novel, and that is the point: none of them requires a new language. Each is the *kind of abstraction* that C exists to support, built once and reused. The argument of Section 6.1—C as a substrate, not a product—is not a promise about what could be built someday; `mls` is what one such substrate looks like once it is built.

### 6.5 Verified, not asserted

The claims above are checkable against the source, which is the standard this paper applies to its own examples. The tag mechanism is in `lib/mls_base.c` and `lib/mls_internal.h` (`REAL_UAF`, `UAF_PROTECTION`); the cleanup attribute and the statistics API are in `lib/mls_ext.h`; the toolkit of Section 6.4 is in `m_tool.h` (`s_*`), `table.h` (`tbl_*`), `list.h`, `m_hdf.c`, and `m_http*.c`. The `mls` test suite and the example programs provide runnable self-checks that assert handle-count baselines and exercise the bounds and teardown behavior; at the time of writing, `lib/mlscoretest.c` builds with AddressSanitizer and exits zero. The small-tool case study of Section 5.5 is not a thought experiment: `procwatch`, `netwatch`, `csv2insert`, `hdf_dump`, and `world` are working programs in the repository—demonstrations, not deployed products—and `docs/guide/lean-code.md` records how the "Python-comfort" primitives above were used to keep them small. A reader who distrusts the prose can build and run all of it. That is the right standard for a systems claim, and it is the standard the rewrite movement applies to its own benchmarks—we should apply it to ours.

### 6.6 C's domain—and where it ends

This argument has a boundary, and it should be stated plainly. C is the natural language for a specific domain: operating systems and kernels, daemons, embedded and real-time systems, database engines, language runtimes, and the tools and libraries that implement them. For that domain the default should be C, and the burden of proof belongs to any language that proposes to replace it. Small Unix tools and monitoring daemons belong on this list in principle, but the examples in Section 5.5 are demonstration programs for `mls`, not deployed tools: they show the technique, not a track record.

Outside that domain the argument changes, and for a concrete reason. Some tasks are best expressed in a language built for *that task alone*—a domain-specific language (DSL). A DSL is not a better general-purpose language; it is a deliberately narrower one: expressive only in its own domain, written with as little redundancy as possible, so that a sentence in it states the problem in the problem's own vocabulary and can be checked at the level of the domain rather than at the level of the machine. That is why SQL, `sed`, `grep`, `awk`, regular expressions, config and query languages, and the many small DSLs for parsing, trading, geometry, and so on survive: each is the notation of its domain, and a general-purpose rewrite—in C, Rust, or Go—can only re-implement that notation, at greater length and with none of the domain-level guarantees. **Choose the language that fits the task.**

The humblest instance is the shell pipeline. Summing one field is a line of `awk`—`awk '{s += $3} END { print s }' access.log`—with no build step, no allocation, and no ownership to reason about. The C version is `fopen`, `getline`, `strtok`, `strtod`, an error branch for each, a `Makefile`, and a binary that prints the identical number: strictly more code, no more capability. Rewriting that `awk` in C—or in Rust, or Go—is not rigor; it is the cost of refusing a declarative tool that already fits. A one-line filter is not a C program waiting to happen, and it never becomes one. The claim here is not "C everywhere." It is that, for the tasks C has always done, C—used properly—remains the justified default until a new language proves otherwise.

---

## 7. Recommendations for System Programming

The argument so far yields a decision procedure rather than a slogan. The recommendations below are deliberately operational: they can be applied to a component without resolving the culture war.

### 7.1 Rules

1. **Default to C for C's domain; depart on evidence, not fashion.** For the domain defined in §6.6—kernels, daemons, embedded and real-time systems, database engines, and language runtimes—C with modern discipline and bounds-checked abstractions is the default. The burden of proof is on the language that proposes to replace it: show, for your threat model, that its benefit exceeds the costs listed in §4.4. Departure is justified in exactly two cases: (i) the task leaves C's domain, where a domain-specific or higher-level language is the natural fit (§6.6); or (ii) the ledger shows a net benefit.

2. **Treat input validation as the real boundary — and bounds-check it in C.** For a network-facing parser, the dominant risk is input validation: path traversal (`../../../etc/passwd`), injection, missing authorization — a logic problem no language eliminates. Its memory-safety component is out-of-bounds access driven by attacker-controlled lengths and indices, and that is precisely what a length-carrying, bounds-checked substrate (`mls`'s `s_*`/`m_*`) closes — provided the parser copies through the checked API rather than a raw `memcpy` on a borrowed buffer, since the tripwire lives at the accessor boundary, not around the raw memory. The right response is a hard, bounds-checked parser in C — not a migration that buys a subset of safety you already have while leaving the logic risk untouched.

3. **For existing C at scale, prefer hardening and incremental abstraction over big-bang rewrites.** The expected value of rewriting working, tested infrastructure is usually negative once loss of assurance and regression risk are counted. Rewrite the component you were already going to rewrite.

4. **Use the tools that already exist.** AddressSanitizer [28], UndefinedBehaviorSanitizer, MemorySanitizer, ThreadSanitizer, fuzzing (libFuzzer/AFL++), static analysis (Coverity, clang-tidy, cppcheck), compiler warnings as errors (`-Werror`), and hardening (`-fstack-protector-strong`, `-D_FORTIFY_SOURCE=3`, CFI, stack canaries, and where available hardware memory tagging) materially reduce risk. These are not a substitute for memory safety, but they are not nothing, and they compose with incremental abstraction.

5. **Adopt length-carrying abstractions; treat raw `strcpy` and `strncpy` as deprecated.** Prefer dynamic, bounds-checked buffers and strings (`mls`'s `s_*` API, `sds`, `GLib`, or your project's own) at every boundary; with a substrate like `mls` (Section 6.4) the same library also supplies the dictionary, list, config, and HTTP primitives that are otherwise the reason teams leave C. This single change removes a large fraction of the buffer bugs from Section 2.

6. **Reduce attack surface — it, not the language, is what makes rewrites safer.** `sudo-rs`'s dependency reduction from 135 to 3 [14] and its feature pruning were at least as important as the language change. Surface reduction is language-independent and cheap, and it transfers to C hardening exactly as it does to a rewrite.

7. **Measure the trade-off and write it down.** Before committing to a migration, estimate: rewrite cost, regression risk, retraining/hiring, dual-toolchain overhead, FFI-boundary risk, churn/longevity risk, and the expected reduction in defects. If the benefit is X and the cost is Y, say so. An unmeasured migration is not an engineering decision.

8. **Keep what C is good at—and put it on the ledger.** Portability, small binaries, predictable execution, a vast installed toolchain and library base, and above all ABI stability, future buildability, and the absence of a runtime to port are load-bearing advantages. A C program built today can plausibly still be built in forty years — you rebuild source, you do not reproduce a toolchain; a rewritten component must re-earn that guarantee from a dependency graph that may not outlive it.

### 7.2 A decision procedure, not a table

A table that maps "component type" to "language" is the wrong instrument. There is no short table for all problems, and a table smuggles in a false premise: that every decision starts from zero, a greenfield team choosing a language. The realistic starting point is the opposite. You already have a C library and C programmers; that is your capital and your default. The question is never "which language should we use?" but "when do we leave the C we already have, and at what cost?" The default is to stay, and the procedure below is a small set of gates. Start from C and ask the questions in order; only a "yes" moves you out of it.

1. **Does the task leave C's domain?** If the logic maps almost one-to-one onto a DSL or a declarative, higher-level notation—`awk`, `sed`, `grep`, regex, SQL, a config or query language, a data-analysis pipeline, a web application (§6.6)—write it in that language. This is not a migration; it was never C's work.
2. **Is it hostile input at a privilege boundary, and are you already rewriting it?** TLS, cryptography, `sudo`-class privilege boundaries: use a memory-safe language and reduce features while you are at it. This is the one case where leaving C is itself the default.
3. **Is it new, network-facing parser code?** Bounds-checked C (`mls`) closes the out-of-bounds risk; the rest is input-validation logic that no language removes. Leave for a memory-safe language only if the ledger (§7.1, rule 7) closes in its favor.
4. **Is shared-memory concurrency with data-race risk the dominant concern?** For genuinely new code, Rust's `Send`/`Sync` is a real advantage; for existing code, C plus ThreadSanitizer and fuzzing is the cheaper path.
5. **Is it a microcontroller or tiny embedded target—say, 8 KB of RAM and no MMU?** C (or Zig or Rust `no_std`). A garbage-collected runtime or a `std`-heavy runtime will not fit in 8 KB; C's stack, static allocation, and a tiny arena will. Eight kilobytes is plenty for C.
6. **Otherwise, stay in C.** Existing daemons behind a boundary, local batch tools, engines, kernels, ordinary embedded systems: harden and adopt bounds-checked abstractions (`mls`'s `s_*`/`m_*`) rather than rewrite. Measure the trade-off and write it down before any migration, and rewrite only the component you were already going to rewrite.

The gates that move you *out* of C are few, and worth keeping in view:

| Leave C when | For |
|---|---|
| The task maps onto a DSL or declarative notation (§6.6) | `awk`, `sed`, `grep`, SQL, config/query, a data or web language—whichever it is |
| Hostile input at a privilege boundary, already being rewritten | A memory-safe language, with feature reduction |
| New, network-facing parser of hostile input, and the ledger closes | A memory-safe language |
| Shared-memory concurrency with race risk, in new code | Rust (`Send`/`Sync`) |

---

## 8. Conclusion: A Provocation, and an Invitation

The title of this paper is *You Do Not Know C*. It is meant the way Dijkstra's title was meant: to make a claim sharp enough to force an argument. Dijkstra did not hate programmers who used `goto`; he wanted the industry to think about control flow. This paper does not hate Rust, Zig, or Go; all three are real contributions, and Section 5.4 names the cases where they are clearly the right answer.

The claim is narrower, and stronger, than "C is fine":

- The memory-safety data is real, and C is genuinely unsafe. Anyone who denies this is not arguing in good faith.
- But the conclusion "therefore replace C" does not follow from it. The failures are failures of *use*; the language determines which errors are representable, not whether the program is correct.
- New languages relocate complexity rather than removing it, and they carry real costs: retraining, dual toolchains, FFI hazards, ecosystem churn, and the discarding of decades of hard-won assurance.
- For many of the same tasks—daemons, tools, embedded systems, kernels—C is not meaningfully harder, and its existing implementations are among the most reliable software ever written.
- Most of the genuinely useful features of other languages can be built in C, incrementally, on top of what already exists. `mls`—a handle library plus a string, dictionary, config, HTTP, and subprocess toolkit (Section 6.4)—is a working existence proof, and the small tools of Section 5.5 are its output; there are many other implementations of the same principle.
- C was always meant for this: not to be a finished framework, but a substrate for building your own abstractions. The correct response to a broken `strcpy` is a length-carrying one, not a new language.
- Memory safety is achievable in C. What no language provides is safety *for free*; the choice is always which costs to pay.

The recommendation is not "always C." It is "stop making language choice a matter of identity and start making it a matter of engineering." For C's domain, the default is C—until the new language proves otherwise. Write down the threat model. Measure both sides of the ledger. Rewrite the components that are already being rewritten and that face hostile input. Harden and abstract everything else. And grant C the respect it has earned: not as a perfect language, but as one that has, for fifty years, let good programmers build good systems—and that will continue to, if we let it.

I am aware that this paper will annoy people on both sides. That is the point. To be unambiguous: this is not an argument against new languages, nor against novelty. It is an argument against novelty that does not solve enough problems to justify its cost. If it moves a few teams from "rewrite everything" or "change nothing" toward "choose, and measure," it has done its job. The debate is worth having. Let us have it with data.

---

## References

[1] Dijkstra, E. W. (1968). "Go To Statement Considered Harmful." *Communications of the ACM*, 11(3), 147–148. DOI: 10.1145/362929.362947.
[2] Thomas, G., & Microsoft Security Response Center (2019). "A proactive approach to more secure code." MSRC Blog, 16 July 2019. https://msrc-blog.microsoft.com/2019/07/16/a-proactive-approach-to-more-secure-code/ (accessed 2026).
[3] Miller, M. (2019). "Trends, challenge, and shifts in software vulnerability mitigation." BlueHat IL 2019. https://github.com/microsoft/MSRC-Security-Research/blob/master/presentations/2019_02_BlueHatIL/2019_01%20-%20BlueHatIL%20-%20Trends%2C%20challenge%2C%20and%20shifts%20in%20software%20vulnerability%20mitigation.pdf (accessed 2026).
[4] The Chromium Projects. "Memory safety." https://www.chromium.org/Home/chromium-security/memory-safety/ (accessed 2026).
[5] Google Security Blog (2019). "Queue hardening enhancements." https://security.googleblog.com/2019/05/queue-hardening-enhancements.html (accessed 2026).
[6] "Apple memory safety" analysis (2019). https://langui.sh/2019/07/23/apple-memory-safety/ (accessed 2026). Cited in the Prossimo survey below.
[7] Prossimo / Internet Security Research Group. "What is memory safety and why does it matter?" https://www.memorysafety.org/docs/memory-safety/ (accessed 2026).
[8] CVE-2014-0160, "Heartbleed" OpenSSL TLS heartbeat extension buffer over-read. https://nvd.nist.gov/vuln/detail/CVE-2014-0160 (accessed 2026).
[9] CVE-2017-0144, Microsoft SMBv1 remote code execution ("EternalBlue"), used by WannaCry. https://nvd.nist.gov/vuln/detail/CVE-2017-0144 (accessed 2026).
[10] "SQL Slammer" (2003), Microsoft SQL Server buffer overflow worm. See e.g. https://en.wikipedia.org/wiki/SQL_Slammer (accessed 2026).
[11] National Security Agency (2022). "Software Memory Safety." Cybersecurity Information Sheet, November 2022. https://media.defense.gov/2022/Nov/10/2003112742/-1/-1/0/CSI_SOFTWARE_MEMORY_SAFETY.PDF (accessed 2026).
[12] Cybersecurity and Infrastructure Security Agency (CISA) et al. (2023). "The Case for Memory Safe Roadmaps." December 2023. https://www.cisa.gov/resources-tools/resources/case-memory-safe-roadmaps (accessed 2026).
[13] Office of the National Cyber Director (ONCD) (2024). "Back to the Building Blocks: A Path Toward Secure and Measurable Software." February 2024. https://bidenwhitehouse.archives.gov/wp-content/uploads/2024/02/Final-ONCD-Technical-Report.pdf (accessed 2026).
[14] Prossimo / Internet Security Research Group. "sudo and su." https://www.memorysafety.org/initiative/sudo-su/ (accessed 2026).
[15] Trifecta Tech Foundation (2025). "Memory-safe sudo to become the default in Ubuntu." https://trifectatech.org/blog/memory-safe-sudo-to-become-the-default-in-ubuntu/ (accessed 2026).
[16] Rust for Linux. https://rust-for-linux.com/ (accessed 2026).
[17] The Rust Programming Language. "Understanding Ownership." https://doc.rust-lang.org/book/ch04-00-understanding-ownership.html (accessed 2026).
[18] Zig Software Foundation. "Why Zig When There is Already C++, D, and Rust?" https://ziglang.org/learn/why_zig_rust_d_cpp/ (accessed 2026).
[19] The Go Programming Language. https://go.dev/doc/ (accessed 2026).
[20] Ritchie, D. M. (1993). "The Development of the C Language." *Proceedings of the Second ACM SIGPLAN Conference on History of Programming Languages (HOPL-II)*, 201–208. DOI: 10.1145/154766.155580.
[21] The Linux Kernel Archives. https://www.kernel.org/ (accessed 2026).
[22] SQLite. https://www.sqlite.org/ (accessed 2026).
[23] Redis. https://redis.io/ (accessed 2026).
[24] memcached. https://memcached.org/ (accessed 2026).
[25] id Software (1993). *DOOM*. See e.g. https://github.com/id-Software/DOOM (accessed 2026).
[26] Miller, T. C., & de Raadt, T. (1999). "strlcpy and strlcat — consistent, safe, string copy and concatenation." *Proceedings of the USENIX Annual Technical Conference*. https://www.usenix.org/legacy/events/usenix99/full_papers/millert/millert.pdf (accessed 2026).
[27] The `mls` C11 handle library and toolkit. Source: this repository (`lib/mls_base.c`, `lib/mls_base.h`, `lib/mls_internal.h`, `lib/mls_ext.c`, `lib/mls_ext.h`, `lib/m_tool.h`, `lib/table.h`, `lib/list.h`, `lib/m_hdf.c`, `lib/m_http.c`, `lib/mlscoretest.c`). (accessed 2026).
[28] AddressSanitizer. https://clang.llvm.org/docs/AddressSanitizer.html (accessed 2026).

## Appendix A — Editorial Note: Seed Claims and Deviations

*This appendix documents the origin of the paper and the points where the argument was corrected against the evidence. It is intended for the author and should be removed before publication.*

The paper grew from a seed argument supplied by the author. Its central claims are preserved, mapped as follows:

| Seed claim | Where it lives |
|---|---|
| The problem is not C but the programmer | Title; §4.1; §8 |
| New languages (Rust, Zig) exist to solve traditional C problems, but pass by the core of the problem and add complexity | §3; §4.2 |
| New-language features are good; the trade-off must be considered | §3.1; §4.4 |
| Decades of C experience and programs have enormous value | §4.3 |
| A C daemon (Redis/memcached) need not be substantially more complex than in Rust/Zig/Go | §5.1; §5.5 |
| sudo in C vs. sudo-rs: marginal safety gain vs. excellent existing infrastructure | §5.2 |
| Linux and DOOM prove C can do the same tasks | §5.3 |
| Most useful features of other languages can be built in C (MLS) | §6.3–6.4 |
| C was always meant for writing your own "language" in C, not just using libc and blaming `strcpy` | §6.1–6.2 |
| Default C for C's domain, burden of proof on a replacement language, DSL/other-domain exception | §6.6; §7.1 |

**Deviations from the seed, driven by evidence:**

1. **`strncpy` is not presented as the safe alternative to `strcpy`.** The seed suggested the fix is `strncpy` ("strcpy without n"). The literature shows `strncpy` is itself error-prone (no guaranteed null terminator, silent zero-fill). The paper reframes the fix as a *length-carrying abstraction*, which is stronger and more accurate.
2. **The memory-safety statistics are cited per source with their scope** (Microsoft 2019, Chromium 2015–2020, Android, Apple, exploited zero-days), not as a single universal "70%".
3. **The `mls` use-after-free mechanism is described as a 7-bit rotating protection tag with a known 128-free wrap ceiling**, verified against `lib/mls_base.c`/`lib/mls_internal.h`, rather than an unbounded "generation counter."
4. **A dedicated strongest-counterargument section (§3.3) and a disclosure of the author's conflict of interest (§4.6) were added**, because a position paper that hides its opponent's best argument and its author's incentives is not persuasive.
5. **A comparison basis and explicit threat models were added to the case studies (§5.0)** so the central empirical claims are testable rather than rhetorical.
6. **The domain and burden-of-proof framing (§6.6, §7.1) sharpens the seed's claim that a new language's cost "rarely justifies" its use into an explicit presumption in favor of C for C's traditional domain, with the burden of proof on the replacement language.** This is a sharpening of the seed, not a deviation from the evidence.
7. **The `mls` toolkit (§6.4) and the small-tool case study (§5.5) were added to give the abstract claim that other languages' features can be built in C a concrete, runnable form**, and the disclosure (§4.6) was expanded to match `mls`'s now-recurring role—including the explicit limitation that its demonstration programs are its own examples in this repository, not evidence of third-party adoption.
