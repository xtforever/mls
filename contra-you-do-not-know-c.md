# The Carriage Manifesto

### Why *You Do Not Know C* is the finest defence of the horse-drawn carriage ever written — in 2026

> The horse is not the problem. The coachman is. Build better reins.
> — the argument of `draft-paper-you-do-not-know-c.md`, restated without its footnotes.

**A hostile reading, written on purpose.** This document is not fair. It is the strongest bad-faith case that can be made against `draft-paper-you-do-not-know-c.md`, assembled so that the author can meet the criticism before it is made by someone less friendly. Every attack below is grounded in the paper's own words and in the source it cites. Where an attack would require a fact the paper does not contain, the fact is either verified in this repository or flagged as external and left unsourced rather than invented.

---

## Contents

1. [The rhetorical shell: concede everything, conclude nothing](#1-the-rhetorical-shell-concede-everything-conclude-nothing)
2. [The Dijkstra inversion](#2-the-dijkstra-inversion)
3. [The strawman the paper promised not to build](#3-the-strawman-the-paper-promised-not-to-build)
4. ["Complexity is conserved" is a category error](#4-complexity-is-conserved-is-a-category-error)
5. [The existence proof that proves the opposite](#5-the-existence-proof-that-proves-the-opposite)
6. [Survivorship bias dressed as evidence](#6-survivorship-bias-dressed-as-evidence)
7. [The ledger that was never written](#7-the-ledger-that-was-never-written)
8. [The FFI argument eats itself](#8-the-ffi-argument-eats-itself)
9. ["Build your own language" is the disease](#9-build-your-own-language-is-the-disease)
10. [The longevity gambit](#10-the-longevity-gambit)
11. [The burden of proof, inverted](#11-the-burden-of-proof-inverted)
12. [The paper tells on itself](#12-the-paper-tells-on-itself)
13. [The kill-shot](#13-the-kill-shot)

---

## Thesis of this takedown

The paper sets out to prove that C is a defensible engineering default. What it actually proves is subtler and worse for it: that when you hand an intelligent, well-read author a foregone conclusion, he will build an apparatus of citations capable of surviving any evidence — including his own. Every statistic in the paper is real. Every concession is sincere. And the machine assembled from them has exactly one output, which is the output the author held before he began: keep writing C.

The horse-carriage analogy is not a cheap shot; it is the paper's own structure. A carriage is not bad because it is old. It is bad because the failure it defends — a horse that can bolt — is the failure the automobile was built to remove. The paper's response is to insist that horses are fine, that the coachman is the problem, that a sufficiently careful driver will never be thrown, and that anyone who buys a car is discarding the accumulated wisdom of the stables. It is a moving and learned defence. It is also a defence of the thing whose entire point was that the moving part was dangerous.

The paper is not the enemy of progress. It is its most persuasive eulogist.

---

## 1. The rhetorical shell: concede everything, conclude nothing

The first tell is structural, and it is not subtle. Open §2 and the author concedes the entire empirical case against C, with the enthusiasm of a man who has decided that honesty is the cheapest form of camouflage:

- roughly **70% of the CVEs Microsoft assigns** [§2.1],
- around **70% of Chromium's serious security bugs**, from 912 high- or critical-severity bugs [§2.1],
- **90% of Android vulnerabilities** [§2.1],
- **60–70% of iOS/macOS vulnerabilities** [§2.1],
- **more than 80% of zero-days exploited in the wild** [§2.1],
- Heartbleed, EternalBlue, SQL Slammer [§2.2],
- the **NSA**, **CISA**, and the **U.S. Office of the National Cyber Director** all recommending a move to memory-safe languages [§2.3].

Section 2 is the honest part of the paper. It is also, by word count, the short part. §3 through §8 run roughly **three-and-a-half times** as long, and their job is to argue that none of the admitted evidence entails the conclusion any reasonable reader would draw from it.

That proportion is the whole trick. A man who concedes the data and then devotes three and a half times as many words to explaining why the data does not mean what it says is not being rigorous. He is **filibustering** — using the *appearance* of concession to buy the authority to dismiss the concession. The paper repeatedly asks to be trusted because it "takes the memory-safety data seriously" [§1]. Taking the data seriously is not a conclusion; it is a promise. This paper makes the promise in §1 and breaks it in §4.

### 1.1 The unfalsifiability engine: "under a given threat model"

The load-bearing clause of the entire argument is introduced quietly and then used to dismiss every piece of evidence against it:

> For a given component, under a given threat model, does the *marginal* reduction in risk from a new language exceed the *total* cost of adopting and living with it—including the cost of discarding working, tested, understood infrastructure? [§3.4]

This sounds like rigour. It functions as a universal solvent. Any counterexample — any memory-safety disaster, any successful rewrite, any kernel that added Rust — can be reclassified as belonging to a *different* threat model and therefore as not applying. The paper even does this to its own statistics in §2.5, dismissing the samples as "not representative" because they are "the hardest case, not the typical case."

Here is the problem: "not representative" cuts both ways, and the paper only ever lets it cut one way. The browsers, kernels, and TLS stacks it dismisses as unrepresentative are precisely the software the paper itself holds up in §4.3 as C's greatest triumph — Linux, OpenSSL, Git, nginx. The same artifacts cannot be the crown jewels of the C ecosystem in one section and an unrepresentative sample in another. Either these systems are what C is *for*, in which case their defect statistics are exactly on point, or they are outliers, in which case the "decades of tested infrastructure" argument in §4.3 collapses. The paper wants both, and the threat-model clause is the door through which it walks between them.

### 1.2 The provocation shield

Finally, the paper pre-emptively immunises itself against the reaction it knows it will provoke:

> I am aware that this paper will annoy people on both sides. That is the point. [§8]

This is not an argument. It is a trap for the reader. It divides all responses into two classes: agreement, and the annoyance that the paper has already declared to be evidence of its own courage. It is the rhetorical equivalent of the playground taunt *I'm rubber, you're glue*. The title does the same work, and the author is candid about the device:

> Its title, *You Do Not Know C*, is also a provocation, not an insult. [§1]

A paper that announces in advance that disagreement proves its point has not opened a debate. It has closed one.

---

## 2. The Dijkstra inversion

Section 1 and §8 both invoke Dijkstra's *Go To Statement Considered Harmful* (1968) as the model for the paper's title and its "intentional provocation." The invocation is the paper's single most self-destructive move, because Dijkstra's argument is the precise opposite of the paper's own thesis.

Dijkstra did not write *Programmers Who Use Goto Considered Harmful*. He argued that the unrestricted jump should be **removed from the language**, because a program whose control flow can go anywhere is hard to reason about. The industry agreed. It did not convene a summit on being more careful with `goto`. It **removed `goto`** — or at least reduced it to a disciplined, structured set of constructs that made the dangerous thing hard to express.

That is the memory-safety argument. Dijkstra's lesson is: when a construct is the recurring source of a class of errors, you change the language so the class of errors cannot be represented. The paper's lesson is: the construct is fine, the programmer is at fault, and the correct response is more discipline. These are not cousins. They are opposites. The paper cites the father of "make it unrepresentable" to defend "keep it and be careful," and it does so in the very first section, apparently without noticing.

### 2.1 "The programmer is the problem" is a permanent, unactionable condition

Strip the thesis to its core:

> C is not the problem. The programmer is… [§4, §8]

Consider what this claim actually delivers. It is not a finding. It is not a policy. It is a statement that will be exactly as true in 2076 as it is in 2026, regardless of what anyone does. "The programmer is the problem" is unfalsifiable by construction, because there will never be a year in which programmers stop making mistakes. It is not a plan for safety; it is a permanent excuse for its absence.

The paper treats this as a serious claim because it is *partly* true — of course humans make errors. But the entire question is what you do about a known, unbounded source of human error. Structural engineering did not respond to collapsing bridges by declaring that the engineer is the problem and founding a discipline of being more careful. It built factors of safety into the material so that a single error could not bring the structure down. Memory-safe languages are the factor of safety. The paper's answer is a stern memo.

And the paper's own conceded data is the refutation. If the 70%-and-80% figures are true — and the paper insists they are — then the failure is not concentrated among careless programmers. It is concentrated among the best-funded, most security-obsessed, most heavily-reviewed engineering organisations on the planet, who deploy fuzzing, sanitizers, static analysis, and review as a full-time discipline and *still* produce memory bugs at that rate. "The programmer is the problem" is not a diagnosis of the profession. It is a diagnosis of the species, offered as a solution.

---

## 3. The strawman the paper promised not to build

> A position paper that strawmans its opponents is worthless. [§2]

So the paper says — and then it commits the exact fallacy it disavowed. In §5.3 it announces:

> the assertion "you cannot do X safely in C" is empirically false for the broad class of X that C has actually been doing for decades. [§5.3]

No serious advocate of memory-safe languages makes that assertion. The claim is not that secure software *cannot* be written in C — everyone agrees it can, with enough care, and the paper's own litany of long-lived C systems is the proof. The claim is that the **defect rate** is higher, the failure modes are worse, and the cost of getting it right is not borne by the language but by every project separately. The distinction is between "possible" and "responsible as a default," and it is the entire argument.

What makes the passage galling rather than merely clumsy is that the paper concedes the point in the very next breath:

> If the claim is instead "you cannot do X *as safely* in C," that is a different, measurable, and much narrower claim — and one that must be weighed against the costs in Section 4.4. [§5.3]

So the paper builds a strong claim, knocks it down with Linux and DOOM, and then, in the same paragraph, admits that the actual claim is the narrow one — which it has not refuted and cannot refute with its evidence. Refuting "you cannot do X in C" tells us nothing about "you can do X more safely elsewhere." It is a demolition of a position nobody holds, performed immediately before conceding that the real position is live. That is not a rebuttal. It is a confession wearing a rebuttal's clothes.

---

## 4. "Complexity is conserved" is a category error

> A new language does not delete the difficulty of systems programming; it relocates it. [§4.2]

This is the paper's most quotable line and its most dishonest move, because "relocates" and "conserved" smuggle in an equivalence that does not exist. The things being compared are not comparable quantities.

What Rust "relocates" onto the programmer is borrow-checker friction: a cost paid **at compile time**, **by the author**, **once**, and which never reaches a user as an incident. What C leaves open is a use-after-free or a buffer overflow: a cost paid **at runtime**, **by users**, **forever**, and which can reach them as remote code execution. Different payer. Different currency. Different side of the ledger.

The paper calls this a change of "distribution": "They change the error *distribution*" [§4.2]. It is not a redistribution; it is a unit conversion that hides the exchange rate. Saying a language "conserves" complexity by trading compile-time annoyance for the elimination of memory corruption is like saying a hospital that cured a disease merely "relocated" it into the billing department. The complexity may be conserved in some abstract accounting. The **harm** is not — and harm is what safety is about. The paper's own §3.2 concedes the asymmetry without noticing: Rust's cost is "engineering time that the project does not get back" — bounded, internal, budgeted. Its benefit is the failure that did not happen to someone else. Equating a team's lost afternoons with a stranger's compromised server is not conservation. It is leverage.

---

## 5. The existence proof that proves the opposite

The centerpiece of the paper is `mls`, a C11 handle library by the same author, offered in §6.3 as an "existence proof" that "the genuinely useful features of other languages can be built in C." It is a real piece of engineering. It is also, read honestly, the strongest argument in print that you should not do this.

The paper's own disclosure names the ceilings (§6.3): a lookup indirection, a 24-bit handle cap, and a 7-bit use-after-free tag that wraps after 128 frees. Those are not incidental caveats. They are the mechanism, and the mechanism is the indictment. Verified against the source in this repository:

- The UAF "protection" is a 7-bit tag: `REAL_UAF(m) = (m >> 24) & 0x7f` (`lib/mls_internal.h:42`). The counter is global and increments on every `m_free` in the process (`UAF_PROTECTION = (UAF_PROTECTION + 1) & 0x7f`, `lib/mls_base.c:739`). After 128 frees it wraps, and a stale handle then has roughly a **one-in-128 chance** of matching a reused slot. This is not a guarantee. It is a tripwire that stops working with predictable frequency.
- On the default path, a UAF mismatch does not return an error. It calls `ERR` → `deb_err` → **`exit(1)`** (`lib/mls_base.c:274`). In a network daemon — the paper's own §5.1 example — a remotely-influenced stale handle is a **kill switch**. The program does not become memory-safe; it acquires a remote denial-of-service primitive. That is a different CVE class, not the absence of one. (The `_SAFE` accessors return an error instead, which merely reintroduces the "did you remember to check?" discipline that was the original complaint.)
- Bounds checking is per-access, at runtime, through a table lookup; the lock exists only in thread-safe builds (`lib/mls_internal.h:10-26`). Rust's bounds checking is compile-time and free at runtime. `mls` buys an approximation of one Rust guarantee by paying the runtime, memory, and CPU cost that Rust's design exists to avoid.
- The paper's own §6.3 admits `mls` "does not, by itself, prevent every possible concurrency race." It prevents **none** of them: data-race freedom is one of Rust's two headline guarantees, and `mls` does not attempt it. So `mls` is not an existence proof that "the genuinely useful features of other languages can be built in C." It is a partial existence proof for one of two features, implemented probabilistically, by one author, with no evidence of a third-party audit.

Put those together and the "existence proof" inverts. To obtain a fraction of what Rust provides for free, you must build a private, runtime, single-author library that is slower, partial, probabilistic, and stops protecting you after 128 frees — and you must do it again for every project, in every codebase, in every language-of-one that C permits. `mls` does not demonstrate that C is enough. It demonstrates that when C needs memory safety, it reimplements Rust — worse, later, and alone. That is not a rebuttal of the memory-safety movement. It is a demonstration of its premise, performed by an opponent.

---

## 6. Survivorship bias dressed as evidence

Section 4.3 is the paper's emotional core: a litany of C's monuments — Linux, SQLite, PostgreSQL, Redis, memcached, nginx, curl, OpenSSL, Git, DOOM — offered as proof that C is "battle-tested." Every item on the list is real. Their joint rhetorical value is close to zero, because the list is selected on the outcome.

Survivorship bias has a textbook definition: reasoning from the things that made it, by ignoring the things that didn't. Section 4.3 is that definition with a bibliography. For every Redis there are thousands of C projects that were abandoned, rotted, or died of their bugs. The list does not show that C produces durable software; it shows that software with decades of resources and maintenance survives — which is true in every language, because surviving is what "with decades of resources and maintenance" means. The graveyard is never mentioned. That is the whole method.

And even the survivors do not cooperate with the thesis:

- **SQLite**, offered as a C success story, is the exception that proves the rule. Its legendary reliability comes from an industrial-scale verification regime — by the project's own account roughly **590 times** as much test code as library code `[external: sqlite.org, testing.html]`. That is not how C is normally written; it is an extraordinary, deliberately unreplicable investment, undertaken precisely because getting this class of code right is hard. The paper holds up the one project that spent a fortune to escape C's failure modes as evidence that C's failure modes are not a problem.
- **Linux**, Exhibit A, is actively hedging against the paper's thesis. It has accumulated **thousands of CVEs** `[external]`, and its own maintainers added a second language to the kernel out of concern for exactly this class of bug: Rust-for-Linux was led by Miguel Ojeda and merged by Linus Torvalds `[external: rust-for-linux.com]`, and maintainers such as Greg Kroah-Hartman have argued publicly that memory-safe languages remove whole categories of defects `[external]`. The paper cites Linux to prove C is enough, in the same essay in which the Linux developers themselves decided it was not.
- **OpenSSL** appears in the §4.3 pantheon as foundational C infrastructure — while §2.2 cites Heartbleed, an OpenSSL defect, as one of the canonical memory-safety disasters [§2.2, §4.3]. The paper's hero list contains its own most famous villain, three sections apart, without a blush.
- **DOOM** is a 1993 single-player video game: no privilege boundary, no network parser, no hostile input, no secrets to steal. It proves that C can render demons for thirty years. It says nothing — nothing — about whether C is a safe default for software that faces attackers. Its appearance in this argument is the moment the essay stops being a position paper and becomes a slideshow.

Finally, the citation itself is padded. The §4.3 sentence lists seven systems — "PostgreSQL, Redis, memcached, nginx, curl, OpenSSL, and Git — each a foundational, battle-tested, widely audited system" — and attaches references [23] and [24], which cover **Redis and memcached only**. Five of the seven systems are asserted with no source at all, under the adjective "widely audited." In a paper whose closing invitation is "let us have it with data" [§8], a five-sevenths unsourced claim is not a small thing. It is the paper's own standard, applied to the paper.

---

## 7. The ledger that was never written

The paper's central methodological boast is that the trade-off is "usually asserted rather than measured" [Abstract], that the movement has produced "excellent data on one side of the ledger and almost none on the other" [§3.3], and that migrations should be decided by writing both sides down:

> Measure the trade-off and write it down. … If the benefit is X and the cost is Y, say so. An unmeasured migration is not an engineering decision. [§7.1]

Now open §4.4, the paper's own cost ledger. It lists eight costs — rewrite cost, loss of institutional assurance, retraining and hiring, dual toolchains, FFI and boundary risk, ecosystem maturity risk, opportunity cost, and churn/longevity risk — and attaches **not one magnitude to any of them**. Eight costs, zero numbers. Not a range, not an order of magnitude, not a single case study with a budget. Meanwhile the benefit side, which the paper concedes at length, carries hard figures: 70%, 70%, 912 bugs, 90%, 80%, 135 dependencies reduced to 3.

So the ledger is not merely unbalanced; it is a blank column with the word *large* written across the top. The paper's only concrete number about a migration — `sudo-rs` going from 135 dependencies to 3 [§5.2] — is a **benefit of the rewrite**, not a cost of it. The single measured quantity in the entire cost argument is evidence for the other side.

This is the paper's own standard, turned around and aimed back at it. It demands that the memory-safety movement quantify its benefits and its costs, and then it declines to quantify its own. It faults others for arguing from sentiment, and then supplies eight felt costs with no arithmetic. If an organization followed §7.1 literally and tried to fill in X and Y, the paper would hand it X with citations and Y as an empty table. An unmeasured migration, the paper says, is not an engineering decision. By that rule, so is this paper.

---

## 8. The FFI argument eats itself

The paper makes two claims that, placed side by side, work against its own recommendation.

Claim one, §4.5: C is the universal substrate. Every memory-safe systems language interoperates through the C ABI; when a project "moves to Rust," it "does not leave the C world; it enters into a long-term relationship with it at every boundary."

Claim two, §4.4 item 5: "Every call across the language boundary is an ownership/ABI hazard, often in `unsafe` code — the very category of risk the change was meant to remove."

Put them together. If memory-safe programs must cross into C at the boundaries, and every crossing is an `unsafe` hazard, then the danger is proportional to the size of the C surface that remains. The rational response is to shrink that surface to the smallest, most-reviewed boundary you can manage — the classic privilege-separation instinct, applied to language. The paper's response is the opposite: **default to C for C's domain** [§7.1], which is precisely the choice that maximises the surface it just called dangerous.

The paper even states the mechanism correctly and then draws the wrong lesson from it: "The more of a system that moves to a new language, the more *boundary* there is…" [§4.5]. That is an argument for migrating deliberately and keeping the boundary thin. It is not an argument for leaving the bulk of the system, and its accumulated unsafety, on the C side of the fence.

To be fair: §7.1 does carve out new, hostile-boundary code for memory-safe languages, so this is not a formal contradiction. But that is exactly the shape of the problem. The paper's default is C; the exception is the dangerous case; and the reason C-as-substrate is dangerous is the very thing the paper offers as C's deepest virtue. The paper has correctly identified the poison and is selling it as the meal.

---

## 9. "Build your own language" is the disease

The paper's positive proposal is its most seductive and its most self-defeating:

> The correct response to `strcpy` is not a new language but a length-carrying one. [Abstract; §6.2]
> C was always meant for this: not to be a finished framework, but a substrate for building your own abstractions. [§8]

Read that as a prescription and it is the history of C's greatest failure, not its vindication. "Build your own" is not a solution; it is the mechanism by which the solution never arrives.

Because that is exactly what the C world has done for fifty years — and here is the result:

- **Standardised but unsafe:** `strcpy`, `strncpy` (which the paper itself demolishes in §6.2, correctly), `sprintf`, `gets`.
- **Safe but unstandardised:** `strlcpy`/`strlcat` (introduced by OpenBSD in 1998 and still not ISO C), `sds`, GLib's `GString`, `bstrlib`, `mls`, and one private buffer type per serious project, for fifty years, in every language-of-one that C permits.

The safe versions share no common standard and no common API, so they cannot be mixed without conversion; every library boundary becomes a conversion boundary, and each "safe" string is safe only within the four walls of the project that built it. This is not a healthy ecosystem. It is a hundred mutually unintelligible dialects, each convinced it is the careful one. The paper calls it a substrate. It is a fragmentation.

The paper even names the true failure and then prescribes more of it: "The industry did not fail to notice the problem. It failed to *standardize* the solution into libc" [§6.2]. Correct — and the remedy for a standardisation failure is to standardise, which is precisely what Rust, Go, and Zig did: they took the length-carrying abstraction, made it part of the language, and made it enforced. The paper is not against the abstraction. It is against the standardisation. It wants every team to hand-roll the length-carrying buffer privately, forever, and then blames nobody when they don't.

That is the circularity at the heart of the essay. The paper's answer to "C makes it easy to write unsafe code" is "build a safe abstraction." The proof that this works is a library the author built. The proof that it does not scale is the fifty-year absence of a shared, standard, enforced version of that library — the absence the paper's own §6.2 laments, and then proposes to continue.

---

## 10. The longevity gambit

The paper's newest argument — that C is not merely well-tested but future-proof — is its most sympathetic, and its most seductive bait-and-switch. It deserves a full answer, because longevity is genuinely C's strongest asset and the paper uses it to cover its weakest flank.

**Longevity is not safety.** Everything in the new §4.3 paragraph is true: C's ABI is the stable interop point, a C program can plausibly still build in forty years, Rust has no stable ABI, Zig is pre-1.0. None of it touches memory safety. A program that compiles for forty years can be a use-after-free for forty years. The paper takes a property C genuinely has and lets it stand in for the property C lacks — the same laundering move as §4, but a different error: not the equation of compile-time friction with runtime harm, but the substitution of an orthogonal virtue. To be buildable is not to be safe.

**The portability claim does the heavy lifting, and it is overstated.** The paper now argues that C has "no runtime to port" and that "Linux userspace famously never breaks," so a K&R-era program can be rebuilt on a modern system "absolutely and without doubt." Three problems. First, "never breaks" is a promise about the *syscall* ABI, not glibc's own symbol and ABI churn, where the friction actually lives — the paper's own caveat admits a newer glibc binary may not run on an older system. Second, "rework the small part that no longer compiles" understates it: a real K&R-era codebase relies on implicit `int`, K&R function definitions, `gets`, and undefined behavior whose meaning has changed — that rework is bounded but not zero, and the paper never prices it. Third, the Go comparison is a category error: Go binaries are statically linked, so a shipped Go program needs no runtime to run; the cost of "rebuilding with a version-locked toolchain" is real, but it is the same recompile-from-source the paper celebrates for C. The asymmetry — port a program versus port a runtime — is real but far smaller than claimed, and the claim of "no doubt" is exactly the kind of confidence the paper refuses to accept from its opponents.

**Rust's missing ABI is a choice, not a defect — and the paper treats it as one.** Rust deliberately has no stable ABI so it can keep evolving the language; what it *does* guarantee is source stability through editions. Source keeps compiling. The absent ABI is paid for by recompiling from source against a pinned or vendored dependency graph — an operation the crate toolchain is built to perform. The paper attacks "no stable ABI" while saying nothing about "stable source semantics," which is the guarantee that actually matters for building the code again. That is cherry-picking, and it is the same trick it accuses the memory-safety movement of: choosing the metric that flatters the desired conclusion.

**The speculation is symmetric.** The claim that crates "must still resolve and compile decades from now" is a prediction about the future with no evidence behind it. So is the claim that C's dependencies will persist. glibc does not guarantee forward compatibility; its ABI has moved. The paper is confident about the next forty years in exactly one direction, and that confidence is as unsupported as the past-tense certainty it mocks in its opponents.

**The new cost is still not measured.** Item 8 is called "churn and longevity risk" and added to §4.4 — and, like the other seven, it carries no magnitude. Zero numbers have become zero numbers plus one. The paper demands that migrations be measured [§7.1 rule 7] and then supplies another felt cost. See §7, above.

**The kill.** Even granting the longevity argument in full, the paper attaches no quantity to it, and its own §2 offers defect *shares and counts*, not rates. So the trade it proposes is a documented present risk — memory bugs, measured in the tens of thousands of CVEs the paper already cites — against an unmeasured future one, "some dependencies might be annoying to rebuild." The paper would rather bet on the forty-year persistence of its crates than on the forty-year persistence of its own demonstrated memory bugs. Section 2 already tells us which of those is the safer bet.

---

## 11. The burden of proof, inverted

The paper's headline recommendation is jurisdictional:

> Default to C for C's domain; depart on evidence, not fashion. … The burden of proof is on the language that proposes to replace it. [§7.1]

A burden of proof is normally placed where the evidence is thin. Here it is placed on the side that has already produced the evidence. The case for memory-safe languages in hostile software is not an intuition awaiting proof; it is the NSA, CISA, and the Office of the National Cyber Director saying so, plus the conceded 70%–90% defect shares and the 80% of exploited zero-days [§2.1–2.3]. The paper responds to a documented, institutionally endorsed pattern by requiring the evidence to be re-produced component by component — while offering, for its own side, no measured costs and no counter-statistics (see §7, above). Demanding proof from the only party that has it, after it has been produced, is not rigour. It is filibustering with a gavel.

The scoping of those institutions matters, and the paper is entitled to say so: the NSA/CISA/ONCD material is guidance for critical and memory-unsafe software, not a command to rewrite every kernel. But "the guidance is scoped" is not "the guidance is wrong," and the paper uses the existence of a threat model to neutralise evidence rather than to weigh it.

Then there is the circularity of the domain itself. Section 6.5 defines C's domain **positively** — kernels, daemons, embedded systems, database engines, runtimes, tools — and §7.1(ii) formally permits departure when "the ledger shows a net benefit." So the frame is not, in the strict sense, vacuous; it has an escape hatch. But the hatch is nailed shut in practice. To use §7.1(ii) you must produce the net-benefit ledger, and §4.4 — the paper's only ledger — carries no magnitudes. The result is a default that cannot be practically overturned by evidence, because the instrument that would overturn it was never built. And the decision the default most discourages is exactly the one that produced the paper's own strongest counterexample: a language-motivated rewrite, undertaken because the risk was judged unacceptable before a rewrite was otherwise planned — which is what `sudo-rs` was [§5.2]. The paper praises the outcome and builds a default that would have prevented it.

---

## 12. The paper tells on itself

The most damaging material in the paper is the material the author chose to include about the paper.

**(a) The appendix documents a drift toward the indefensible.** Appendix A maps the seed claims and records the "deviations driven by evidence." Deviation 6 is candid: the seed's claim that a new language's cost "rarely justifies" its use was "sharpened" into the explicit presumption that C is the default and the replacement carries the burden of proof [Appendix, deviation 6; §7.1]. So the editorial process did not soften the thesis under scrutiny; it hardened it. And the resulting presumption is one whose discharge path (§7.1(ii), the net-benefit ledger) the paper never operationalises, because §4.4 carries no magnitudes. The paper's own record shows it moving from a claim that could be argued to one that cannot be practically discharged — and filing that movement under "sharpening, not deviation."

**(b) The paper ships its own working notes.** Appendix A is marked "*It is intended for the author and should be removed before publication.*" A document confident in its rigour does not circulate a page announcing which parts are internal scaffolding. The appendix exists in the draft the reader is holding, which means the reader can see the seed list that the polished prose was built to defend. The essay's polish is retrospective; the seed is the actual argument.

**(c) The claim to "follow the evidence" does not survive the framing.** The authorship note promises that "where a verified source contradicts an initial intuition, the paper follows the evidence" [Note on Authorship]. The narrow instance is honoured: the seed's `strncpy` fix was corrected, and §6.2 now presents `strncpy` as the trap, not the remedy [Appendix, deviation 1]. But the **framing** does not follow the evidence. The paper repeatedly reduces the entire memory-safety problem to libc string handling — "the correct response to `strcpy`," "raw libc string handling is bad" [Abstract; §6.1] — when its own §2 establishes that the dominant and most dangerous classes are use-after-free, out-of-bounds access, and lifetime errors in complex code, not over-long strings. The seed intuition was "C's problem is `strcpy`, and the fix is a better string copy." The evidence is that the problem is broader and the fix is structural. The paper corrected the example and kept the frame. That is a portrait of an author defending a conclusion, not following evidence — even as the note claims the opposite.

**(d) The conclusion points at the author's product.** Section 4.6 discloses that the author of the paper is the author of `mls`. That disclosure is honourable, and the paper asks to be judged on its reasoning rather than its library. But look at what the reasoning is *for*. The paper's existence proof is `mls` (§6.3). Its "build your own abstractions" remedy is `mls`-shaped (§6.1–6.2). Its cost ledger, as shown above, is empty. Its recommendation is "default to C." Every load-bearing exhibit in the essay is a dependency-free demonstration that you do not need to adopt something someone else built — written by the person who built it. Strip away the citations and the structure is an advertisement for a private C library, with a literature review attached. The disclosure does not neutralise that; it names it.

None of this makes the author dishonest. It makes the paper predictable: a defence of the status quo, written by an interested party, whose own appendix records the process of becoming more certain rather than less. That is not what a position paper should look like when it is working.

---

## 13. The kill-shot

If a hostile reviewer had to compress this into a single paragraph, it would read:

> *You Do Not Know C* is a beautifully written, thoroughly footnoted argument that C is not the problem, which opens by conceding every statistic that shows it is, then spends three and a half times as long arguing that the statistics mean the opposite, under a threat-model clause that makes any counterexample dismissible and a provocation shield that makes any objection proof of the author's courage. Its central example of "memory safety in C" is a single-author library whose use-after-free protection is a 7-bit tag that wraps every 128 frees and whose default response to a stale handle is `exit(1)` — a remote kill switch in a network daemon — written by the same author, held up as proof that you do not need Rust. Its supporting evidence is a survivor list that omits the graveyard, includes the project (OpenSSL) that gave the paper its own worst example (Heartbleed), and counts a 1993 video game as a systems-safety credential. It measures neither side of the ledger it demands others measure, then adds an eighth unmeasured cost it calls "churn and longevity risk"; it calls the private-fragmentation it prescribes a "substrate" and inverts the burden of proof onto the only side that has produced evidence. The paper's own epigraph is Dijkstra — who argued for making dangerous constructs impossible — used to defend the thesis that the programmer is at fault. It is the finest argument for moving on from C that anyone has ever accidentally written, and it does not know it.*

The author is free to ignore all of this. But the criticism will be made, and it will be made in this register — less fairly, less carefully, and with fewer receipts. The strongest thing this document can do is let the paper hear the case before it is argued by someone who has stopped listening.
