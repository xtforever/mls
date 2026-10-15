# Safety Is Purchased, Not Declared

### A synthesis of *You Do Not Know C* and *The Carriage Manifesto*

**Status:** Synthesis  
**Date:** 2026

---

## 1. Preamble

This document is a synthesis of two papers: `draft-paper-you-do-not-know-c.md` (the **thesis**, a position paper arguing that C remains a defensible engineering default), and `contra-you-do-not-know-c.md` (the **antithesis**, subtitled *The Carriage Manifesto*, a deliberately hostile reading assembled so that the thesis could meet its strongest critic). It assumes both are available; it does not re-litigate every footnote.

A synthesis is not a compromise. Splitting the difference between two positions produces something neither side would defend. The aim here is to identify what each paper got *right that the other missed*, discard what each got *wrong*, and state the resulting position more precisely than either original could.

The synthesis has one central move. Both papers argue, directly or indirectly, about *which language should be the default*. This document replaces that question with one that has a determinate answer: **where is this component's safety cost paid, and by whom?** Once the question is asked that way, most of the apparent disagreement dissolves, and the part that remains is measurable.

---

## 2. The common ground both papers already occupy

Both papers accept the same facts, and it is worth stating them once so that the rest of this document can treat them as settled.

Memory-safety defects dominate serious vulnerabilities in large, hostile-target C/C++ codebases: roughly 70% of Microsoft's CVEs, around 70% of Chromium's serious bugs (912 analyzed since 2015), about 90% of Android's, 60–70% of Apple's, and more than 80% of exploited zero-days. Heartbleed, EternalBlue, and SQL Slammer were memory-safety failures. The NSA, CISA, and the U.S. Office of the National Cyber Director recommend memory-safe languages for critical software. The mechanisms are out-of-bounds access, use-after-free, unbounded copy, and lifetime errors.

Both papers also accept the *procedure*: decisions should be made component by component, under an explicit threat model, with the costs written down rather than asserted. The thesis says so directly in §3.4 and §7.1; the antithesis does not dispute the procedure, only the thesis's use of it.

This is a substantial agreement. It means the two papers do not disagree about facts or methods. They disagree about **framing** and **defaults**. That is where a synthesis has to work.

---

## 3. Where the thesis is right, and the antithesis overreaches

The antithesis is deliberately unfair, and it is right about more than the thesis would like. But it overcorrects in four places, and each correction matters for the final position.

### 3.1 The cost side is real, and neither paper measured it

The thesis's strongest methodological point is that the memory-safety movement has produced excellent data on the benefit side of the ledger and almost none on the cost side (§3.3). The antithesis correctly observes that the thesis's own §4.4 lists eight costs and attaches no magnitude to any of them (contra §7).

But the antithesis makes the same mistake it is prosecuting. Its "graveyard" — the "thousands of C projects that were abandoned, rotted, or died of their bugs" (contra §6) — is asserted, not measured. No project is named, no rate is estimated, no comparison is made. So the honest state of the field is this: **the cost side of language migration is unmeasured by both sides.** The thesis is right that this is a gap; the antithesis is right that the thesis did not fill it; and the antithesis then declines to fill it either. The synthesis draws the conclusion neither paper draws: the correct contribution is not a number, but a *specification of which numbers would be needed* (§6 below), together with the refusal to pretend that either paper has produced them.

### 3.2 Threat models genuinely differ

The antithesis calls "under a given threat model" an "unfalsifiability engine" (contra §1.1). It is right that the thesis uses the clause as a universal solvent, reclassifying every counterexample as belonging to a different threat model. But the antithesis overcorrects into the opposite error: it treats the *existence* of scope distinctions as evasion.

The distinction between a browser parsing hostile input from the entire Internet and a local batch converter transforming files the user owns is not a rhetorical dodge. It is the entire content of risk analysis. The synthesis keeps the threat model, but demotes it from a get-out-of-jail clause to a **required input**: it must be *specified and defended*, not merely invoked, and it may be used to scope which data applies, never to dismiss data outright.

### 3.3 Longevity is a maintenance asset, not a safety argument

The antithesis is right that "buildable for forty years" does not touch memory safety: a program can compile for forty years and be a use-after-free for forty years (contra §10). But it dismisses the longevity point too quickly. Longevity is a real *economic* term — the cost of keeping a system buildable and maintained — and the thesis is right that the rewrite ledger usually omits it.

The synthesis reclassifies rather than discards: **longevity belongs in the cost ledger as a maintenance-cost advantage of C, not in the safety case.** The thesis's error was letting it stand in for safety; the antithesis's error was throwing it away because it is not safety.

### 3.4 `mls`'s real value is ergonomics, which the antithesis ignores

The antithesis reads `mls` exclusively as a failed memory-safety mechanism — the 7-bit tag, `exit(1)`, runtime bounds checks (contra §5). Its technical dissection is correct (see §4.3, below). But it never mentions the string API, the sorted dictionary, HDF configuration, HTTP, or subprocesses. That is the actual reason a working programmer would use the library, and it is a strawman in the opposite direction.

The synthesis states the honest split: **`mls` is valuable as ergonomics, and its memory-safety claim is the weak part.** The antithesis is right about the weak part and wrong to pretend there is nothing else.

---

## 4. Where the antithesis is right, and the thesis is wrong

These are not marginal. They are load-bearing, and the synthesis accepts them.

### 4.1 The Dijkstra inversion

The thesis invokes Dijkstra's *Go To Statement Considered Harmful* as the model for its own intentional provocation (thesis §1, §8). The antithesis shows that this is the single most self-destructive citation in the paper, because Dijkstra's argument is the opposite of the thesis's.

Dijkstra did not write *Programmers Who Use Goto Considered Harmful*. He argued that the unrestricted jump should be **removed from the language**, because a construct that is the recurring source of a class of errors should be made unrepresentable. The industry did not respond by being more careful with `goto`; it removed it or reduced it to structured constructs. That is precisely the memory-safety argument.

The synthesis adopts the antithesis's reading without qualification. "Considered harmful" means **make the dangerous class unrepresentable**, not "keep the construct and be careful." The corollary is important and cuts against both papers' more optimistic claims: **in C you can make a class of errors *harder* to represent, never *impossible*.** That residual difference — harder versus impossible — is exactly the quantity a real ledger must price.

### 4.2 "Complexity is conserved" is a category error

The thesis's most quotable line — "a new language does not delete the difficulty of systems programming; it relocates it" (§4.2) — conflates two different currencies.

Borrow-checker friction is paid **at compile time, by the author, once**, and never reaches a user as an incident. A use-after-free is paid **at runtime, by users, potentially forever**, and can reach them as remote code execution. Same noun ("complexity"), different payer, different time, different currency. Calling that "conservation" hides the exchange rate.

The synthesis replaces the slogan with a sharper formulation: **harm is not conserved; it is redistributed — and a redistribution across different payers and different currencies is not a wash.** The unit of account becomes *who pays, when, and in what currency*. That is the ledger's real schema.

### 4.3 `mls` is a tripwire, not a guarantee

The antithesis's technical dissection of `mls` is correct and checkable against the source:

- the use-after-free "protection" is a 7-bit tag (`REAL_UAF(m) = (m >> 24) & 0x7f`) whose global counter increments on every `m_free` and **wraps after 128 frees**, at which point a stale handle has roughly a one-in-128 chance of matching a reused slot;
- on the default path a stale handle does not return an error — it calls `exit(1)`, which in a network daemon is a **remote denial-of-service primitive**, a different CVE class, not the absence of one;
- bounds checking is per-access and at runtime, through a table lookup, whereas Rust's is compile-time and free at runtime;
- it does not attempt data-race freedom at all — one of Rust's two headline guarantees.

This does not refute `mls`'s *usefulness*. It refutes `mls`'s *safety claim*, and the synthesis accepts the distinction. `mls` is relocated in §8.

### 4.4 The ledger has no numbers

The thesis's own methodological boast is that migrations should be decided by writing both sides down (abstract; §3.3; §7.1). Its cost ledger lists eight costs and attaches not one magnitude (contra §7). The antithesis is right. A blank column with "large" written across the top is not a measurement. §6 specifies what a real ledger requires.

### 4.5 "Build your own" is the mechanism by which the standard never arrives

This is the antithesis's strongest structural point (contra §9). The C world has, for fifty years, produced safe string APIs — `strlcpy`/`strlcat`, `sds`, GLib's `GString`, `mls`, and one private buffer type per serious project — that share no common standard and no common API. Each is safe only inside its own project. That is fragmentation, not a substrate.

The thesis itself diagnoses the true failure correctly: "the industry did not fail to notice the problem. It failed to *standardize* the solution into libc" (§6.2). The synthesis accepts the implication: **the remedy for a standardization failure is standardization.** Rust, Go, and Zig took the length-carrying abstraction, made it part of the language, and made it enforced. That is not an argument that C must be abandoned; it is an argument that "build your own" is an *ergonomic and local* answer, not a *safety and global* one. Its value is real, but it is not the value the thesis claimed for it.

### 4.6 Survivorship bias and the padded pantheon

Section 4.3 of the thesis is selected on the outcome. For every Redis there are thousands of C projects that did not survive; the list shows that software with decades of resources and maintenance survives, which is true in every language. And even the survivors do not cooperate: SQLite's legendary reliability rests on an industrial-scale test investment the project itself measures at roughly 590 times its library code — an extraordinary, deliberately unreplicable effort, not how C is normally written; Linux has accumulated thousands of CVEs and added Rust to the kernel out of concern for exactly this bug class; OpenSSL appears in the pantheon in §4.3 while Heartbleed, an OpenSSL defect, is the thesis's own canonical disaster in §2.2; DOOM has no privilege boundary, no network parser, and no hostile input, and proves that C can render demons for thirty years; and the sentence listing seven "widely audited" systems attaches sources for two of them.

The thesis's §4.3 survives only in weakened form: as evidence that C **can** host durable systems given extraordinary investment — not that C makes durability ordinary.

### 4.7 "The programmer is the problem" is unfalsifiable

The thesis's core slogan — "C is not the problem. The programmer is" (§1, §8) — is a permanent, unactionable condition. It will be exactly as true in 2076 as in 2026, whatever anyone does. It is not a finding or a policy; it is a structure that cannot be refuted. And the thesis's own data refutes it as a diagnosis: if 70–90% figures are true, the failure is concentrated among the best-funded, most-reviewed engineering organizations on the planet, not among careless individuals.

The synthesis drops the slogan. Its replacement is falsifiable and specific: **the question is not who is at fault, but where the safety cost is paid and by whom.** That question can be answered with a ledger.

---

## 5. The resolution: two axes and a symmetric burden

The two papers are not really arguing C versus Rust. They are arguing past each other about **where the safety cost lives**. Named correctly, the disagreement resolves.

### 5.1 Two axes

Every component can be placed on two independent axes.

**Axis A — Is memory safety load-bearing for this component?**
*Load-bearing* means: hostile input crossing a trust boundary, a privilege boundary, secrets at stake, or shared-memory concurrency where data races are the dominant risk. *Background* means: local files, trusted input, closed networks, internal batch work.

**Axis B — Where is the safety cost better paid?**
*Centralized* means the cost is paid once, in the compiler or type system, and amortized across every user of the language. *Distributed* means the cost is paid per project, in a hand-built abstraction, at runtime, with coverage that is partial and probabilistic.

### 5.2 The rule

- **Load-bearing → centralize.** For a component whose failure is a security incident, a language guarantee that makes the error class unrepresentable dominates a per-project abstraction that makes it harder to express. This is not a close call, and it does not require running a cost ledger first. It is where the antithesis is simply right.
- **Background → either.** When safety is a background risk rather than a security property, both approaches are defensible, and then C's genuine advantages — maintenance cost, ABI stability, the existing tested substrate, small binaries — legitimately decide. This is the surviving, weakened form of the thesis's domain argument.

### 5.3 The symmetric burden of proof

The antithesis shows that the thesis inverts the burden of proof: it places proof on the side that has already produced the evidence, and demands nothing of its own side (contra §11). The thesis's "burden on the replacement language" is an unjustified presumption in favor of the incumbent.

The synthesis rejects default-by-incumbency as firmly as it rejects default-by-novelty. The burden is **symmetric and rests on every claim of safety sufficiency**:

- If you claim "C plus a hand-built abstraction is safe enough for component X," you owe the residual-risk measurement: what the tripwire catches, what it misses, and what remains after sanitizers and fuzzing.
- If you claim "Rust is safe enough for component X," you owe the migration-cost measurement and the new defect distribution, including the `unsafe` and FFI surface you will still own.

This one move dissolves the inversion, and it also dissolves the antithesis's implicit assumption that the cost question is already settled. It is not settled. It is unmeasured on both sides.

### 5.4 The test on two components

Two examples, chosen to fall on opposite sides of Axis A, show that the rule produces different answers without contradiction.

**Component A — a new TLS termination proxy.** It parses hostile input from the entire Internet, holds session secrets, and sits at a privilege boundary. Axis A is unambiguously *load-bearing*, and the rule returns *centralize*. The ledger is not consulted to decide *whether* to use a memory-safe language; it is consulted afterward, to choose among them and to budget the migration. The antithesis wins this component outright, and the thesis's §5.4 already concedes it.

**Component B — a local CSV-to-SQL converter** run by one analyst on files they own, never network-exposed. Axis A is *background*. Axis B is close either way, and the real costs are migration (retraining, dual toolchain, new dependency graph) against the maintenance and longevity advantages of an existing C toolchain. The ledger decides, and it will usually close in favor of C — not because C is safe, but because the risk is background and the alternatives cost more. The thesis wins this component, in the weakened form of §3.3 and §4.6.

Neither paper's *default* is right; the *classification* is. Set the default from the threat model, then measure the rest. That is the whole procedure, and it is the reason the two papers can both be partly correct about the same language.

---

## 6. The ledger, actually written

Both papers demand that the trade-off be measured. Neither measures it. The contribution of this synthesis is not a result but a **schema** — the set of quantities a real ledger would contain. Until they exist, any confident recommendation, in either direction, is rhetoric.

### 6.1 What the ledger requires

1. **Defect rates, not shares.** The 70–90% figures are statements about the *mix* of defects within particular codebases, not about defects per unit of engineering effort (thesis §2.5 is right about this). The question the ledger needs is: for the same component and threat model, how many exploitable memory-safety defects per engineer-month does each approach produce? Nobody in either paper produces this number.
2. **Cost magnitudes.** Rewrite cost, retraining and hiring, dual-toolchain overhead, FFI-boundary risk, ecosystem churn, and longevity risk must be estimated in engineer-months or currency — or explicitly marked "not measured." Eight felt costs are not a ledger (contra §7).
3. **Residual risk of the abstraction approach.** For a C component with a hand-built abstraction, what does the abstraction actually guarantee? `mls`'s answer is instructive and humbling: bounds checks catch one class; a 7-bit rotating tag catches the *immediate* use-after-free until it wraps at 128 frees; the default response is `exit(1)`, which is availability loss, not memory safety; and data races are unaddressed. The residual after sanitizers, fuzzing, and review is the number that matters.
4. **The payer matrix.** For each side, record *who pays* (author, compiler, user, operator), *when* (compile time, runtime, incident time), and *in what currency* (engineering time, availability, confidentiality, integrity). This operationalizes §4.2's correction: compile-time friction and runtime exploitation are not the same unit and cannot be summed naively.
5. **A named comparison basis.** Same component, same threat model, same feature set — the thesis's own §5.0 standard, applied symmetrically to both sides of the ledger.

A payer matrix for a single component makes the shape of the ledger concrete:

| Cost event | Payer | When | Currency |
|---|---|---|---|
| Borrow-checker friction | Author | Compile time | Engineering time (bounded, budgeted) |
| Buffer overflow / use-after-free in a hand-built C abstraction | Users, operator | Runtime | Confidentiality, integrity, availability |
| UAF tripwire firing as `exit(1)` in a daemon | Operator | Runtime | Availability (a denial-of-service primitive) |
| Migration retraining and dual toolchain | Organization | Transition and ongoing | Engineering time |
| `unsafe` / FFI boundary mistake in the new language | Author | Runtime | Integrity |

The table is the point: rows one and two are not the same unit, which is exactly why "complexity is conserved" cannot be a sum. Compile-time friction is paid by the author in budgeted time; a runtime memory-safety failure is paid by someone else in compromised systems. A ledger that adds them is not measuring; it is laundering.

### 6.2 The honest state

The thesis supplies hard numbers on the benefit side and a blank cost side. The antithesis supplies no numbers at all — only a demand that the thesis produce them. The memory-safety movement, in the thesis's own words, has "produced excellent data on one side of the ledger and almost none on the other" (thesis §3.3), and the antithesis does not contest this; it only notes that the thesis then fails to supply the other side.

So the field's true state is: **benefits are documented, costs are asserted — by everyone.** This synthesis does not fill that gap. It names it, and it refuses to pretend the gap has been closed by either paper.

---

## 7. The corrected recommendation

The decision procedure below is what remains after the corrections of Sections 3–6. It replaces the thesis's "default to C, burden on the replacement."

1. **Specify the threat model and the trust boundary.** If you cannot, stop — the decision is not yet an engineering one. The threat model is an input, not an escape hatch.
2. **Classify safety as load-bearing or background** (Axis A).
3. **If load-bearing: choose the memory-safe language.** Do not run the ledger to avoid this step. The ledger exists to decide the background cases, not to relitigate the security-critical ones.
4. **If background: write the ledger** (Section 6) — payer matrix, magnitudes, comparison basis. Choose C unless the ledger closes against it.
5. **In every case, reduce surface.** Cut features, cut dependencies, cut the boundary area. Surface reduction is language-independent and was the most durable lesson of `sudo-rs`.
6. **Re-evaluate when the threat model changes.** New network exposure, new privilege, new concurrency, or new secrets move a component from background to load-bearing. The classification is a property of the deployment, not of the file.

### 7.1 The corrected decision table

| Component / situation | Recommendation | Why |
|---|---|---|
| New, network-facing parser of hostile input | Memory-safe language | Load-bearing → centralize. Not a ledger question. |
| New TLS / crypto / privilege-boundary tool | Memory-safe language; reduce features | Load-bearing; least room for error |
| Shared-memory concurrency, races are the risk | Rust if greenfield; C + TSan/fuzzing if existing | Centralized race freedom is only available in the language |
| Existing C daemon behind a boundary | Harden + incremental abstraction, **explicitly not sold as memory safety** | Background risk; assurance exceeds rewrite benefit |
| Existing C at a privilege boundary, already slated for rewrite | Memory-safe rewrite, with feature reduction | Load-bearing, and the rewrite was already planned |
| Embedded / bare-metal / real-time | C, Zig, or Rust `no_std` by requirement | GC runtimes are disqualified; explicit memory matters |
| Local batch / data tools | C with an ergonomics library | Background risk; migration cost rarely pays |
| Greenfield, no legacy, hostile input | Memory-safe language | Centralizing the safety cost is cheapest before there is code |

---

## 8. What `mls` actually is

Strip the safety claim and the library that remains is genuinely useful, and it is a different thing from what the thesis presented.

- **An ergonomics library.** Strings (`s_*`), a sorted dictionary (`tbl_*`), lists and trees (`list_*`), an S-expression configuration format with a writer (`m_hdf`), HTTP client and embedded server with config-driven routing (`m_http`, `m_http_server`, `m_flask`), subprocess helpers (`m_subproc`), and single-file amalgam headers. This is the "high-level language features in C" claim, and it is real. The antithesis never engages it; the thesis oversold it by attaching the safety claim.
- **A development-time tripwire.** Bounds checks, a rotating use-after-free tag, deterministic recursive teardown that survives cycles, and leak statistics. Useful while developing. **Not a runtime guarantee**, and not a memory-safety mechanism in the Rust sense: the tag wraps every 128 frees, the default failure response is `exit(1)`, bounds checks are runtime costs, and data races are untouched.

The honest claim is therefore narrower than the thesis's and more useful than the antithesis allowed:

> **C can be made *productive* — strings, dicts, configuration, HTTP, and subprocesses in one small dependency — and a careful team can add development-time tripwires. That is not "C can be made memory-safe."**

The distinction matters because the thesis leaned on `mls` for both claims and only the first survives. The library remains a legitimate demonstration that the *ergonomic gap* between C and high-level languages is closable. It is not a demonstration that the *safety gap* is closed. And it remains the author's own library, from the author's own repository; the disclosure stands, and it should be read as "here is one person's attempt at the ergonomics term," not "here is safety in C."

---

## 9. Conclusion

> **The debate is not C versus Rust. It is centralized versus distributed safety, load-bearing versus background risk — and on that axis the two papers stop contradicting each other and start measuring.**

The thesis was right that the safety trade-off is an engineering decision and that its cost side is unmeasured; it was wrong to defend C by default, to conflate a tripwire with a guarantee, and to cite Dijkstra against Dijkstra. The antithesis was right about the mechanisms, the inversion, and the empty ledger; it was wrong to treat every scope distinction as evasion and to dismiss the ergonomic and maintenance terms entirely.

The synthesis keeps the economics of the thesis and the mechanics of the antithesis. It drops the unfalsifiable slogan ("the programmer is the problem"), the category error ("complexity is conserved"), the inverted burden of proof, and the safety claim made for `mls`. What remains is smaller than either paper wanted, and it is the first version of the argument that can be tested.

---

## Appendix A — The antithesis, point by point

Every substantive attack in `contra-you-do-not-know-c.md`, and the synthesis's resolution.

| # | Antithesis attack | Resolution |
|---|---|---|
| 1 | Rhetorical shell: concede everything, conclude nothing | Accepted as a warning, not a verdict. The synthesis replaces concession-plus-argument with the two-axis test, which yields a determinate answer per component. |
| 2 | The Dijkstra inversion | **Accepted fully.** "Considered harmful" means make the class unrepresentable; in C it can only be made harder. |
| 3 | The strawman ("you cannot do X safely in C") | Accepted. The live claim is about defect rate and failure modes, not possibility; the synthesis drops the strawman and states the rate question. |
| 4 | "Complexity is conserved" is a category error | **Accepted.** Replaced by "harm is not conserved; it is redistributed across payers and currencies." |
| 5 | The `mls` existence proof proves the opposite | Accepted on the safety claim, **rejected on the ergonomics claim.** `mls` is reclassified in §8. |
| 6 | Survivorship bias | Accepted in part. §4.3 survives only as evidence that C *can* host durable systems with extraordinary investment. |
| 7 | The ledger that was never written | **Accepted.** §6 supplies the schema; the synthesis admits neither side has the numbers. |
| 8 | The FFI argument eats itself | Accepted. The corrected recommendation reduces boundary area in every case (§7, step 5). |
| 9 | "Build your own" is the disease | Accepted. It is an ergonomic/local answer, not a standardization/safety one. |
| 10 | The longevity gambit | Accepted that longevity is not safety; **rejected** that longevity is therefore irrelevant. It is kept as a maintenance-cost term in the ledger. |
| 11 | The burden of proof, inverted | **Accepted.** Replaced by a symmetric burden resting on every claim of safety sufficiency. |
| 12 | The paper tells on itself | Accepted as evidence of framing drift. The synthesis is explicit about the seed and about the author's interest in `mls`. |
| 13 | The kill-shot | Treated as the strongest hostile summary; its true parts are absorbed into Sections 4 and 8, and its overreaches are named in Section 3. |
