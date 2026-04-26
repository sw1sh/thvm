# waldmeister/ summary and IC-native ATP sketch

Research-and-design memo. Out of scope: actual reimplementation. The
deliverable is a summary of Waldmeister's algorithms, a survey of
existing IC-and-ATP work (April 2026), and a sketch of how an
IC-combinator-native equational prover could be built on top of thvm
using the SupGen / NeoGen idea.

## Context

`waldmeister/` is a vendored copy of Waldmeister ("woodruff"; the herb),
a high-performance ATP for first-order unit equational logic, developed
1994-2001 in the SEKI / Univ. Kaiserslautern group. It implements
**unfailing Knuth-Bendix completion** as a semi-complete proof procedure:
given a finite set `E` of equations, decide `s = t`. Its algorithms (CP
generation, ordering-driven orientation, discrimination-tree indexing)
are widely used in modern equational ATPs (E, Vampire, Twee).

We want to (a) understand what Waldmeister actually does, (b) survey the
existing landscape of IC-and-ATP work, and (c) sketch how the same proof
procedure could be expressed as **IC graph rewrites** in thvm,
particularly leveraging the SupGen / NeoGen idea (superpose all candidate
inferences and let optimal sharing collapse the search tree).

**Terminology warning.** "Superposition" means *two completely
different things* on the two sides of this project: HVM-SUP (a
runtime data primitive, the IC node `&L{a, b}`) versus
ATP-superposition (a logical inference rule, refined paramodulation).
The plan below uses the former to encode the search-space of the
latter, but they live at different layers.  See the *Equational
reasoning and the IC-as-ATP layer* section of
[../glossary.md](../glossary.md) for a complete table including
related terms (paramodulation, critical pair, bisubstitution / cosubstitution
in Wolfram's sense, unification, joinability, saturation, ...).

German-name conventions are translated inline on first use. Module
abbreviations: `INF` = *Inferenz* "inference"; `NF` = *Normalform*
"normal form"; `ORD` = *Ordnungen* "orderings"; `CLAS` = *Klassifikation*
"classification"; `WDT` = *Waldmeister-Diskriminierungs-Baum*
"discrimination tree"; `TPR` = *Termpaar-Repraesentation* "term-pair
representation"; `WASIC` = mixed-acronym basic term operations;
`MNF` = *Multi-Normalform*.

---

## 1. What Waldmeister does (one-page summary)

**Input**: many-sorted signature, a reduction ordering (KBO or LPO), a
finite set of equations `E`, and a conjecture `s = t` (or set of
conjectures). See
[waldmeister/documents/ShortDocumentation.txt](../../waldmeister/documents/ShortDocumentation.txt).

**Main loop** in
[sources/INF/Hauptkomponenten.c](../../waldmeister/sources/INF/Hauptkomponenten.c)
(*Hauptkomponenten* = "main components"), function `HK_Vervollstaendigung`
(*Vervollstaendigung* = "completion"): a saturation cycle over two sets,
`R` (selected oriented rules) and `CP` (waiting critical-pair queue):

1. **Select** a CP from the priority queue
   ([sources/INF/KPVerwaltung.c](../../waldmeister/sources/INF/KPVerwaltung.c);
   *KPVerwaltung* = *Kritische-Paare-Verwaltung*, "critical-pair
   management").
2. **Normalize** both sides under `R` to canonical form
   ([sources/NF/NFBildung.c](../../waldmeister/sources/NF/NFBildung.c);
   *NFBildung* = *Normalform-Bildung*, "normal-form construction").
3. **Trivialize / orient**: discard if equal; otherwise orient by
   ordering.
4. **Interreduce** `R` against the new rule
   ([sources/INF/Interreduktion.c](../../waldmeister/sources/INF/Interreduktion.c);
   *Interreduktion* "interreduction" -- a cognate).
5. **Generate** all critical pairs (overlap unification) of the new
   rule with `R`
   ([sources/INF/Grundzusammenfuehrung.c](../../waldmeister/sources/INF/Grundzusammenfuehrung.c);
   *Grundzusammenfuehrung* = *Grund-Zusammenfuehrung*, "ground-merging /
   ground unification";
   [sources/INF/Unifikation1.c](../../waldmeister/sources/INF/Unifikation1.c)
   "unification 1").
6. **Insert** the new rule and goal-test (rewrite the conjecture under
   `R`).

**Key data structures**

- *Term*: applicative tree, flat cell pool, per-rule variable banks
  ([sources/WASIC/](../../waldmeister/sources/WASIC/),
  [sources/TPR/](../../waldmeister/sources/TPR/)).
- *Discrimination tree*: symbol-prefix index for fast rule lookup
  during normalization and matching
  ([sources/WDT/DSBaumOperationen.c](../../waldmeister/sources/WDT/DSBaumOperationen.c);
  *DSBaumOperationen* = *Diskriminierungs-Baum-Operationen*,
  "discrimination-tree operations";
  [sources/WDT/DSBaumTest.c](../../waldmeister/sources/WDT/DSBaumTest.c)
  "DS-tree test").
- *CP priority queue*: heuristic-weighted heap; `--add` (size sum) and
  `--mix` (size + orientability) selection strategies
  ([sources/CLAS/ClasHeuristics.c](../../waldmeister/sources/CLAS/ClasHeuristics.c);
  *ClasHeuristics* = "classification heuristics").
- *Reduction ordering*: KBO
  ([sources/ORD/KBO.c](../../waldmeister/sources/ORD/KBO.c)) and LPO
  ([sources/ORD/LPO.c](../../waldmeister/sources/ORD/LPO.c)); both must
  be total on ground terms in proof mode.
- *PCL proof object*: step-wise inference record for post-hoc
  rendering ([sources/INF/pcl.c](../../waldmeister/sources/INF/pcl.c)).

**Core algorithmic primitives** (the things any reimplementation must
own): unification with binding stack, one-way matching, KBO/LPO
comparators, overlap-position enumeration, term normalization to NF,
subsumption, interreduction, weighted CP queue, goal narrowing /
rewriting, proof trace.

---

## 2. Prior art (web survey, April 2026)

### 2.1 Existing equational / saturation provers

- **Twee** (Smallbone, CADE 28 / 2021): open-source equational prover
  in ~5,300 lines of Haskell, unfailing KB with **ground joinability**
  and **connectedness** redundancy criteria; placed 2nd at CASC-J10
  UEQ. The most useful single reference point for "what good looks
  like" in this niche.
  ([Twee paper](https://link.springer.com/chapter/10.1007/978-3-030-79876-5_35),
  [github.com/nick8325/twee](https://github.com/nick8325/twee))
- **Vampire**: superposition-based, AVATAR architecture, polymorphic
  FOL with theories and induction; the heavyweight general-purpose
  saturation prover. ([vprover.github.io](https://vprover.github.io/),
  [Vampire Diary CAV'25](https://arxiv.org/html/2506.03030v3))
- **E-prover**: superposition with strong indexing; the performance
  baseline for many CASC divisions.

### 2.2 Saturation x synthesis (closest conceptual cousin to NeoGen)

- **"Program Synthesis in Saturation"** (Hozzova, Kovacs, Norman,
  Voronkov; CADE 29 / 2024) -- *extends the superposition calculus so
  that proving validity simultaneously synthesizes a recursion-free
  program*. Implemented in Vampire. Uses unification + answer literals,
  no interaction nets. This is the existing-art version of "use ATP
  search to do synthesis"; conversely, NeoGen flips it ("use synthesis
  runtime to do ATP search").
  ([arxiv.org/abs/2402.18962](https://arxiv.org/abs/2402.18962))

### 2.3 Equality saturation (the e-graph branch)

- **egg** (Willsey et al., POPL 2021): efficient e-graph library; gave
  equality saturation its modern toolchain. Originally e-graphs were
  ATP technology; egg repurposed them for compilers.
  ([dl.acm.org/doi/10.1145/3434304](https://dl.acm.org/doi/10.1145/3434304))
- **"Guided Equality Saturation"** (Koehler et al., POPL 2024): Lean 4
  tactic that uses egg with human-provided guides; reports **2 OOM**
  speedups on group/ring proofs. Direct evidence that
  "saturate-then-extract" beats "search-then-rewrite" in practice for
  unit-equational fragments.
  ([Lean 4 paper PDF](https://goens.org/publications/koehler-popl-24/popl24.pdf))
- **"Semantic Foundations of Equality Saturation"** (Suciu, ICDT 2025):
  reframes saturation in database-theoretic terms; useful if we want a
  cleaner formal model for what an IC-encoded saturator is doing.
  ([drops.dagstuhl.de](https://drops.dagstuhl.de/storage/00lipics/lipics-vol328-icdt2025/LIPIcs.ICDT.2025.11/LIPIcs.ICDT.2025.11.pdf))

### 2.4 Interaction-net side

- **Lafont, "Interaction Combinators"** (Inf. & Comp. 1997): three
  symbols, six rules, the foundation. Also a model in which one can
  encode linear-logic proof nets, so there's a precedent for "use IC
  as a proof representation," even if not for ATP search.
  ([sciencedirect](https://www.sciencedirect.com/science/article/pii/S0890540197926432/pdf))
- **Fernandez & Mackie, "Interaction nets and term rewriting systems"**
  (TCS 1999): the formal bridge -- any orthogonal first-order TRS
  compiles to interaction nets. Straight-line evidence that "express a
  TRS in IC" is not novel; the open question is whether *the search
  loop on top of it* benefits from IC.
  ([sciencedirect](https://www.sciencedirect.com/science/article/pii/S0304397597000820))
- **Lamping (POPL 1990) / Levy**: optimal lambda reduction. SupGen's
  speedup claim is fundamentally "Levy-optimal sharing across the
  search tree." ([Lamping](https://dl.acm.org/doi/10.1145/96709.96711))
- **MLIR Inet Dialect** (Open Design Meeting, April 2025): declarative
  rewrite rules over interaction nets in MLIR; signal that the IN
  community is actively building production tooling, not just academic
  prototypes.
  ([slides PDF](https://mlir.llvm.org/OpenMeetings/2025-04-10-Inet-Dialect.pdf))

### 2.5 HVM / Bend / SupGen / NeoGen

- **HVM4 + SupGen native integration** (Sept 2025): Taelin claims
  SupGen is built into HVM4, with a native dependent type system on
  the interaction net, and that "HVM will be usable as a fast,
  parallel proof verifier... OOMs faster than Lean." Throughput
  cited: **HVM3 ~160M, HVM4 ~130M interactions/s.**
  ([X status 1971591584916393984](https://x.com/VictorTaelin/status/1971591584916393984))
- **NeoGen** (proprietary, 2025): closed evolution of SupGen. Claimed
  to find every primitive recursive function tested "instantly"
  (Equality 0.8 ms, DrawLine 1 ms, Insert 6 ms). Type enumeration as
  first-class superposed objects. *We have only Taelin's posts and
  the open-source SupGen ancestors as evidence.* Treat performance
  numbers as upper-bound claims, not validated benchmarks.
- **ADD-CARRY benchmark** (Taelin gist): **2^16 candidates, 36 K vs.
  262 M interactions, 7,277x speedup, sub-1 interaction per guess.**
  This is the most concrete evidence that DUP-SUP commutation
  actually delivers Levy-optimal sharing on a structured search.
  ([gist](https://gist.github.com/VictorTaelin/d5c318348aaee7033eb3d18b0b0ace34))
- **"Scaling HVM towards an Optimal Theorem Prover"** (Taelin, gist):
  the position paper. Long on ambition, short on concrete ATP
  comparison; invokes "bitter lesson" + 512 Mac Minis. Useful as
  motivation, not as technical roadmap.
  ([gist](https://gist.github.com/VictorTaelin/a060db7bada170e50d61871a752daf6e))

### 2.6 Adjacent: combinator-based superposition

- **Bentkamp, Blanchette, et al., "A Combinator-Based Superposition
  Calculus for Higher-Order Logic"** (IJCAR 2020): different sense of
  "combinator" (S, K, I), but it's the well-known mainstream version
  of "ATP based on combinators." Worth knowing about precisely so we
  don't accidentally imply we mean *that* when we say "IC combinators."
  ([Springer](https://link.springer.com/chapter/10.1007/978-3-030-51074-9_16))

### 2.7 What this survey tells us

- **The general idea -- encode TRS in IC -- is 25+ years old**
  (Fernandez & Mackie). The novel piece is **using SUP/DUP for the
  search**, not just the term representation.
- **No published, peer-reviewed ATP based on interaction-net
  superposition exists.** Taelin's claims are in gists/tweets and the
  closed NeoGen. We would be in genuinely uncharted territory; the
  closest validated references are Twee for the algorithm and HVM4's
  ADD-CARRY for the search-encoding.
- **The strongest existing competition for the use case** ("prove
  unit equalities, fast") is Twee. Expect Twee's tricks (ground
  joinability, connectedness, demodulation, AC redundancy) to dominate
  the actual work distribution; a naive IC re-encoding without these
  will lose.
- **Equality saturation in Lean (Koehler '24) shows "saturate then
  extract" works** for the Lean group/ring fragment with 2 OOM
  speedups over manual rewriting. Structurally close to what an
  IC-native saturator would do; egg has a 5-year head start.
- **Risk specific to the SupGen approach**: Taelin's gists demonstrate
  speedups on *highly structured* search spaces (each guess bit
  independent). Critical-pair generation is *not* obviously that
  structured -- overlap positions and rule choices interact. The
  7,277x number on ADD-CARRY may not transfer.

---

## 3. IC-native ATP sketch (SupGen / NeoGen flavor)

The core bet, copied from
[TinyHVM/resources/supgen_kernel_search.md](../../TinyHVM/resources/supgen_kernel_search.md)
and
[TinyHVM/resources/neogen_deep_dive.md](../../TinyHVM/resources/neogen_deep_dive.md):
*don't enumerate inferences; superpose them and let optimal sharing
reduce them together.* Two complementary uses of SUPs:

**(A) Superpose the rule set.** Represent
`R = &L0{r0, &L1{r1, &L2{r2, ...}}}` as a single labeled SUP. The
normalizer becomes `nf(R, t) = wnf(rewrite_step(R, t))` applied
recursively. Each rewrite step is a single APP-SUP commutation that
fans out to all rules' rewrite candidates simultaneously; failed
candidates collapse to ERA. With *unordered* SUPs (deferred -- neither
HVM4 nor TinyHVM has them yet) the whole rule set is O(1) shareable
across many positions: the analog of a discrimination tree, intrinsic
to the heap.

**(B) Superpose the substitution / overlap space.** Critical-pair
generation walks every overlap position in a rule's LHS against every
other rule. That's a cross product
`&L_pos{positions} x &L_other{rules}`. With distinct labels, collapse
enumerates the full grid; the unification check itself is a pure
function `unify : term x term -> Maybe subst`. Building the grid is
one APP application of `unify` to the superposed pair. Failed branches
become ERA; surviving branches collapse to the produced critical pair.

**(C) Selection and pruning.** Waldmeister's CP priority queue with
`--mix` heuristic = INC nodes wrapping the cost-weighted candidate.
The priority-aware collapse (TinyHVM `thvm_collapse_ordered`)
enumerates cheapest-first, exactly matching "select smallest CP next."
Top-K extraction is just the first K collapse outputs.

**(D) Orientation and the ordering.** KBO/LPO comparators are pure
functions on term pairs -- encode them as IC programs. `orient(s, t)`
returns `Just (s -> t)`, `Just (t -> s)`, or `Nothing`. The same
function consumes superposed equation candidates: orientation across
the superposition is one IC application.

**(E) Goals.** Goal `s = t` is checked by `EQL(nf(R, s), nf(R, t))` --
one EQL node, automatically duplicated across superposed rule
applications. Existential goals (Skolemized) become `ANY` wildcards
which match freely under EQL.

**(F) Proof trace.** This is the awkward part of going IC-native:
optimal sharing actively *destroys* the inference DAG, since each
shared step is counted once. To get a proof object, decorate every
rewrite step with an identifier `(reason_tag, parent_ids, subst)`
packed into an ALO constructor and threaded as state alongside the
rewritten term. Collapse then yields per-branch traces. Cost: linear
in the proof length, not in the search space. (Same shape as
Waldmeister's PCL trace recording, expressed as an IC tuple.)

This is closer to *equational saturation by superposition* than to a
literal port of unfailing KB. You don't need a CP-queue data
structure at all if the SUP-representation of the active rule set is
the queue. That's the NeoGen-style dissolution of explicit search.

**Honest caveats**

- **Fairness.** KB completion needs every overlap to eventually be
  selected, but SUP collapse is not breadth-first by default.
  INC-driven priority + bounded collapse work-stealing should emulate
  fairness, but this is the part most likely to need empirical tuning.
- **Twee's redundancy criteria are not free in IC.** Ground
  joinability and connectedness require comparing terms across the
  whole rule set; the natural IC encoding (one big SUP query) might
  or might not preserve Twee's pruning power. Expect a measurable
  performance gap until we port these explicitly.
- **No validated benchmarks.** All published IC-based search wins are
  on highly structured spaces (ADD-CARRY, Peano sort). CP generation
  is less obviously decomposable. The asymptotic story is plausible;
  the constants are not yet established.

---

## 4. What thvm has vs. needs

### Has today

- 7 IC tags: APP, LAM, VAR, ERA, DP0/DP1, SUP, DUP
  ([src/thvm.h:62-87](../../src/thvm.h#L62)).
- 5 interactions: APP-LAM, APP-ERA, DUP-SUP (same-label only),
  DUP-ERA, DUP-LAM ([src/interact/](../../src/interact/)).
- WNF stack-machine reducer with step budget
  ([src/wnf/_.c](../../src/wnf/_.c)).
- Redex enumeration via heap scan
  ([src/wnf/redex.c](../../src/wnf/redex.c)).
- Tensor / UOp / autograd layer (orthogonal here, but relevant if we
  ever do schedule-search with a learned cost model).

### Missing IC primitives -- all exist in TinyHVM, ready to port

| Need | TinyHVM source | Why |
|---|---|---|
| DUP-SUP **cross-label** commutation | TinyHVM/src/interact/ | sketch (A), (B); current [src/interact/dup_sup.c](../../src/interact/dup_sup.c) is annihilation-only |
| `TAG_EQL` (structural equality) | TinyHVM/src/inet/ | sketch (E), goal-test |
| `TAG_AND` / `TAG_OR` short-circuit | TinyHVM/src/inet/ | combine multi-goal constraints |
| `TAG_ANY` wildcard | TinyHVM/src/inet/ | existential / Skolem variables |
| `TAG_MAT` + constructors C00..C15 | TinyHVM/src/inet/ | ADT term encoding, operator-symbol dispatch |
| `TAG_INC` priority + priority collapse | TinyHVM/src/inet/ | sketch (C), CP selection heuristic |
| `thvm_collapse{,_grouped,_ordered,_par}` | TinyHVM/src/inet/_.c | the actual enumeration API |
| Unordered SUP/DUP (`USP`/`UDP`) | not in HVM4/TinyHVM yet | desirable (linear vs. quadratic on shared rule set); **defer** |
| ICC Bridge/Ann | TinyHVM has `TAG_BRI`, `TAG_ANN` | only for type-directed synthesis later; **defer** |

### ATP-layer code -- greenfield, neither thvm nor TinyHVM has it

- Term encoding for first-order signatures (constructor tags + var
  tags).
- KBO and LPO comparators as IC programs.
- Unification: as IC program, or as a C primitive callable via
  `TAG_PRI` (HVM4 does this for hot kernels; see
  [TinyHVM/resources/neogen_deep_dive.md sec.8](../../TinyHVM/resources/neogen_deep_dive.md)).
- Critical-pair builder (`overlap x rule x rule`) over superposed
  inputs.
- Goal-state encoding + collapse-driven proof extraction.
- Parser for Waldmeister `.pr` specs (or hand-encode test cases at
  first; example at
  [waldmeister/documents/example.pr](../../waldmeister/documents/example.pr)).
- **Twee-class redundancy criteria** (ground joinability,
  connectedness): required to be competitive on UEQ; nontrivial to
  port.

---

## 5. Suggested staged path (if/when we decide to build it)

Each stage ends with a runnable demo. None of this commits before the
user opts in -- this section exists so the plan has a concrete
trajectory.

1. **Port the IC primitives** (week-scale). Bring over `EQL`,
   `AND`/`OR`, `ANY`, `MAT`, `INC`, constructors, plus DUP-SUP
   cross-label commutation and the `thvm_collapse*` family. Each
   lands as a separate `src/interact/*.c` + test, mirroring AGENTS.md
   conventions. **Demo**: a SUP over two candidate naturals, EQL
   against an expected value, collapse to the matching one.
   *(Reproduces the smallest piece of ADD-CARRY's mechanism on
   thvm.)*

2. **Encode terms and one ordering** (week-scale). Pick a tiny
   signature (group axioms from
   [waldmeister/documents/ShortDocumentation.txt](../../waldmeister/documents/ShortDocumentation.txt)).
   Hand-encode equations as IC ADTs. Implement KBO as a pure IC
   program. **Demo**: orient `f(x, e) = x` correctly under a chosen
   precedence.

3. **One-shot rewriter** (week-scale). Given a static `R` represented
   as `&{r0, &{r1, ...}}` and a term `t`, return the normal form by
   APP-SUP commutation + EQL-on-applicability. **Demo**: normalize
   `f(a, e)` to `a` under group axioms.

4. **Critical-pair generation as a superposition** (week-scale).
   Build the overlap x rule x rule grid, apply unification (initially
   as a `TAG_PRI` C primitive), collapse to the surviving CPs.

5. **Saturation loop driven by collapse** (multi-week). Outer loop:
   normalize current goal; if not closed, expand `R` with one CP from
   priority collapse; repeat. **This is where fairness questions get
   real.** Compare against Twee on a few small TPTP-UEQ problems
   (group, ring) to get an honest baseline.

6. **Proof trace + parser**. Add the per-branch trace tuple. Add a
   Waldmeister spec parser so we can run TPTP-UEQ end-to-end.

7. *(Optional)* **Twee-class redundancy criteria**. Ground
   joinability, connectedness, AC redundancy. Deferred until 1-6 are
   working; the naive saturator is the right baseline first.

**Stop conditions** between stages: each one passes its demo; we can
quit at any stage and still have shipped useful IC infrastructure
(steps 1-3 are valuable on their own for SupGen-style program search,
regardless of ATP).

---

## 6. Verification (for the *understanding* deliverable)

This memo's primary deliverable is sections 1-4 (summary, prior art,
sketch, gap analysis); the staged path in sec.5 is the conditional
next step. Verification of *understanding* is:

- Re-read sections 1, 3 against
  [waldmeister/documents/ShortDocumentation.txt](../../waldmeister/documents/ShortDocumentation.txt)
  and the cited C files; confirm the 6-step main loop and the role
  of orderings.
- Re-read section 4 against [src/thvm.h](../../src/thvm.h) and
  `ls TinyHVM/src/inet/`; confirm the cited tags and collapse
  functions exist.
- Sanity-check the SupGen claims against
  [TinyHVM/resources/supgen_kernel_search.md](../../TinyHVM/resources/supgen_kernel_search.md)
  sec.1 and
  [TinyHVM/resources/neogen_deep_dive.md](../../TinyHVM/resources/neogen_deep_dive.md)
  sections 1, 3, 4.
- Treat NeoGen performance claims as upper bounds; the validated
  benchmark we have is **ADD-CARRY: 7,277x, sub-1 interaction per
  guess**. Twee-class equational provers remain the right baseline.

If we do execute sec.5, per-stage verification is the demo described
in that stage; aggregate verification is "prove a small group axiom
example end-to-end and compare wall-clock vs. Twee."
