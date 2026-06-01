# thvm/atp — Coverage Roadmap

What the engine covers today and the open arcs that would extend it
further.  For the algorithms shipped, see [algorithms.md](algorithms.md);
for the implementation, [engineering.md](engineering.md).

The WL `Method` / preset / portfolio surface that wraps these
controls lives in `wl/THVMLink/docs/Tutorials/ATP.md` and
`AtpMethods.md`.  This document tracks what's open at the C-engine
level — features that the WL preset language can't dispatch to
because the engine doesn't implement them yet.

## Coverage today

* Unit-equational reasoning (single equation per axiom).
* Knuth-Bendix unfailing completion (orientable + unorientable
  rules).
* Reduction orderings: KBO (`src/kbo`), LPO (`src/lpo`); both with
  automatic precedence (`src/atp/precedence.c`).
* Term indexing: discrimination tree over rule LHSs
  (`AtpRuleIndex`), FV index over queued CPs (`AtpFvIndex`).
* Redundancy filters: trivial join, forward rule-subsumption,
  queue subsumption, permutation subsumption (AC top-level),
  ground joinability, Bachmair-Dershowitz connectedness.
* Selection heuristics: ADD / MAX / ORD / GT / MIX / MIX2 / UNIF
  / GOAL / TWEE / STAGGERED / RELLEVEL / DIVERSITY / CONJSYM
  weight modes, with periodic FIFO / random / goal-directed /
  K-D-secondary alternation.
* Mandatory Normal Form (MNF) — opt-in eager normalization
  variant.
* LRS (Limited Resource Strategy) — wall-time-aware queue
  pruning.
* AtpFt — parallel native flatterm representation (cells,
  arenas, match, splice, normalize, discrim tree, CP queue)
  gated behind env flags, live-verified to byte-equivalence
  with the Term path.
* AC declarations + canonical-form flattening (`src/atp/ac.c`)
  — Stage 1 of the AC arc below.  No live caller yet.

## Open arcs

In rough order from "closest to what we have" to "different
shape".  Each arc lands behind a build/env flag so the default
engine stays unchanged at every step.

### AC reasoning during rewrite

`src/atp/ac.c` provides per-engine AC info (auto-detected from
the axiom set), AC flattening to a leaf multiset, and AC
canonical form (right-associative chain over hash-sorted leaves).
Open sub-arcs from this base:

* AC equality and AC hashing — `atp_ac_eq(s, t) → bool` and a
  hash invariant under AC.  Wire into trivial-join and
  perm-subsumption.
* AC matching during rewrite — pattern `f(x, y, z)` matches
  subject `f(a, b, c)` under AC iff there's a permutation σ
  making the multisets agree.  Poly-time for unit AC patterns;
  backtracking for shared vars.  Wire into
  `atp_rewrite_normalize` behind `THVM_ATP_AC=1`.
* AC superposition — for AC symbols generate the extended
  "merge position" overlaps needed for AC-completeness.
* AC-KBO and AC-LPO — orderings that respect AC canonical form
  (Steinbach for AC-KBO; Bachmair-Plaisted for AC-LPO).
* Bench — standard AC theory benchmarks (group, ring, lattice);
  differential against Waldmeister on these.

Why this is the natural next arc:

* Largest payoff per LOC.  Handles algebraic theories the current
  engine has to enumerate commutative variants of.
* Fits the saturation loop unchanged — still unit-equational, just
  with a new equivalence-modulo during matching and ordering.
* Standard references: Bachmair-Plaisted for ordering, Peterson-
  Stickel for completion; Waldmeister and Twee both implement.
* Differential-testable: each AC operation has a clear "no-AC"
  version (the current engine) to compare against on AC-free
  workloads.

### Additional reduction orderings

Today: KBO and LPO.  Others worth shipping:

* **RPO** (Recursive Path Ordering) — strict precedence-only,
  handles deeper structural reductions where KBO weights fail.
  LPO is a special case (lex status); MPO is the multiset-status
  variant.
* **PO** (Polynomial Ordering) — assigns each symbol a polynomial
  interpretation.  Strong on arithmetic-flavored problems.
* **WPO** (Weighted Path Ordering) — generalizes KBO + LPO.

The structural lever is a configurable ordering interface (rather
than the current KBO/LPO-only `atp_compare`).

### Full first-order clauses

Today: single-equation axioms.  Open: clauses (vectors of literals
with signs).  Requires:

* Clause representation (literal vector, sign bits, sort).
* Resolution inference (binary resolution + factoring).
* Paramodulation (equality on a literal).
* Equality factoring + reflexivity resolution.
* Selection function (which literal in a clause to inference on).
* Subsumption-resolution (cheaper than full subsumption for
  clauses).
* Optional: splitting (case analysis on disjunctive clauses).

This is the biggest single arc — it changes the input language.
Output: a Vampire/E-class prover instead of a Waldmeister-class
one.

### Skolemization + CNF + reflection

A goal `∀x ∃y P(x, y) → Q(x)` is not a unit equation.  Open work:
the standard FOL-to-CNF pipeline (NNF, Skolemization with the
matrix-tracking outer quantifier, Tseitin/structural CNF) on top of
the clausal extension above.

### Theory reasoning

Built-in theories so the prover doesn't have to derive arithmetic
facts from axioms:

* Linear integer arithmetic.
* Lists, arrays, records.
* Uninterpreted functions vs theory functions.

Standard interface: SMT-LIB or TPTP-TFF input + a theory dispatcher.

### Inference selection refinements

* **Literal selection** — Vampire-style maximal-literal selection
  to reduce inference fan-out.
* **Avatar-style splitting** — propositionally split a clause's
  ground sub-clauses.
* **Inference scheduling** — interleave resolution, paramodulation,
  demodulation per a configurable strategy.

### Saturation algorithms

* **DISCOUNT** — current shape (the given-clause loop with
  passive/active sets).
* **Otter** — explicit `usable` / `sos` queues with different
  selection.  Predates DISCOUNT; partially supported via
  `set_use_sos`.
* **Limited-Resource Strategy** — partially implemented
  (`set_use_lrs`, queue pruning by predicted reachability).
* **Conditional rewriting** — Horn-clause-only reasoning when the
  input fits.

### Proof reconstruction

PCL-style trace with one record per inference, replayable into a
verifier (Coq, Lean, Isabelle).  thvm has a trace skeleton
(`s->trace[]`) but the inference detail isn't yet complete for full
reconstruction.

## Open work-items inside the current coverage

### Bench levers identified by profiling

These don't add new theorems, they speed up the existing engine.
Inputs from a 20 s AndAssoc profile under all AtpFt flags:

* `kbo_subtree_memo` keyed by Term pointer; rekey to shape-hash
  would lift the 35% hit rate toward 70% and unlock the LPO
  orient cache's intended payoff.
* `atp_dt_*` legacy discrim tree + `acp_unpack_term` byte queue
  run alongside the AtpFt paths under dual-store.  ~25% of
  self-time directly removable once every byte-queue consumer
  (FV-index keying, CP-graph mirror, peek/stash) migrates onto
  the AtpFt entries.
* `atp_ft_unorient_step` returns "no fire" on 96% of its calls
  on saturating workloads.  The pos-memo catches 84% of the
  repeat calls, but each empty call still pays a flatten + memo
  lookup.  Real lever: smarter selection that avoids inserting
  CPs whose normalize touches every unorient rule.

### Selection-strategy gaps

* The CP-set interreduction is single all-or-nothing.
  Waldmeister fires different "strength" bits (rules-only /
  equations / subsume / weight / orphan-murder) at different
  cadences on a sample-point schedule.  Porting the schedule
  framework would let cheap operations (orphan-murder) fire
  more often than expensive ones (full reweight).
* CP-selection bias against unorientable rules.  On AndAssoc the
  unorientable-rule fraction grows 21% → 35% over the run; CPs
  whose normalize yields an unorientable rule starve the
  cracker.  An LPO-orientability probe at CP-selection time
  could de-prioritize these.
* Goal-directed selection is interleaved via `set_goal_interleave`
  but doesn't condition on the goal's structure beyond symbol
  overlap.  A real CP-In-Goal / Goal-In-CP weight would
  fingerprint the conjecture and bias by subterm depth.

### Trace + proof completeness

The proof DAG is built (`s->trace[]`) but the per-step inference
records don't yet carry enough detail to reconstruct a Coq/Lean
proof.  Specifically: the `before / after` Term pair for each
rewrite step is recorded under `set_record_norm_steps`, but the
substitution applied isn't.  Adding the σ to every TRACE_NORM
record would close this.
