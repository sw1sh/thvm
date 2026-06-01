# thvm/atp - Coverage Roadmap

What the engine covers today and the open arcs that would extend it
further.  For the algorithms shipped, see [algorithms.md](algorithms.md);
for the implementation, [engineering.md](engineering.md).

The WL `Method` / preset / portfolio surface that wraps these
controls lives in `wl/THVMLink/docs/Tutorials/ATP.md` and
`AtpMethods.md`.  This document tracks what's open at the C-engine
level - features that the WL preset language can't dispatch to
because the engine doesn't implement them yet.

## Coverage today

* Unit-equational reasoning (single equation per axiom).
* Knuth-Bendix unfailing completion (orientable + unorientable
  rules).
* Reduction orderings: KBO (`src/kbo`), LPO (`src/lpo`), RPO
  (`src/rpo`); KBO and LPO with automatic precedence
  (`src/atp/precedence.c`).  RPO adds per-symbol LEX/MUL status
  (LPO is the all-LEX special case; MPO is all-MUL).  Wire into
  ATP via `thvm_atp_set_rpo`.
* Term indexing: discrimination tree over rule LHSs
  (`AtpRuleIndex`), FV index over queued CPs (`AtpFvIndex`).
* Redundancy filters: trivial join, forward rule-subsumption,
  queue subsumption, permutation subsumption (AC top-level),
  ground joinability, Bachmair-Dershowitz connectedness.
* Selection heuristics: ADD / MAX / ORD / GT / MIX / MIX2 / UNIF
  / GOAL / TWEE / STAGGERED / RELLEVEL / DIVERSITY / CONJSYM
  weight modes, with periodic FIFO / random / goal-directed /
  K-D-secondary alternation.
* Mandatory Normal Form (MNF) - opt-in eager normalization
  variant.
* LRS (Limited Resource Strategy) - wall-time-aware queue
  pruning.
* AtpFt - parallel native flatterm representation (cells,
  arenas, match, splice, normalize, discrim tree, CP queue)
  gated behind env flags, live-verified to byte-equivalence
  with the Term path.
* AC reasoning (`src/atp/ac.c`) under `THVM_ATP_AC`:
  - AC symbol declarations + auto-detect from axioms.
  - AC canonical form (right-assoc, hash-sorted leaves).
  - `atp_ac_eq` / `atp_ac_hash` (AC-modulo equality / hash).
  - `atp_match_ac` (AC-modulo one-way match; greedy
    multi-pass-with-leftover-chain).
  - `atp_ac_unify_emit_cps` (Stickel-style recursive AC unifier
    feeding `cp_visit`; handles variable-absorbs-set, |S| != |T|
    asymmetric unifiers via FVR absorption of any non-empty leaf
    subset; bidirectional, up to 8 leaves per side + 64 CPs / call).
  - `atp_ac_extend_rule` (Bachmair-Plaisted `R+`) used as a
    BILATERAL extension in `atp_overlap_ij`: emits CPs over
    (i-ext X j), (i X j-ext), and (i-ext X j-ext) face combos.
  - `atp_kbo_ac` / `atp_lpo_ac` (canonical-form AC orderings).
  Wired into `atp_ordered_try_top`, `atp_rewrite_normalize_ordered`,
  `atp_overlap_ij`, `thvm_atp_goal_check`, `atp_compare_uncached`,
  `cp_visit`.  Gated by `THVM_ATP_AC` build define + non-zero
  `ac_mask` (engine-global; set via `thvm_atp_set_ac_mask` /
  `thvm_atp_auto_ac`).
  Differential vs wmcli on `tests/test_atp_ac_bench`:
  - commutative-monoid (waldmeister/commutative_monoid.pr): thvm
    <0.1ms vs wmcli 2ms (AC-eq goal-check short-circuit).
  - abelian-group inversion (waldmeister/abelian_group.pr): thvm
    PROVED in 4 iters / 1 surviving rule; wmcli 12 rules / 90 CPs.
  - lattice idempotence (waldmeister/lattice_idem.pr): thvm PROVED
    in 4 iters / 4 rules; wmcli 5 rules / 24 CPs.
  - commutative ring 0*a=0 (waldmeister/ring_zero.pr): thvm PROVED
    in 4 iters / 1 rule; wmcli 24 rules / 308 CPs.

## Open arcs

In rough order from "closest to what we have" to "different
shape".  Each arc lands behind a build/env flag so the default
engine stays unchanged at every step.

### AC reasoning follow-ups

Coverage (see "Coverage today" above) lands the core: AC
match + canon, AC-KBO/AC-LPO, extended overlap, AC-eq goal-check.
Remaining sub-items:

* Bench fan-out beyond lattice idempotence: ring axioms (commutative
  + distributive), boolean-ring (`x*x = x` simplification), Robbins
  algebra (Hilbert-prize problem -- gates on goal-directed search +
  AC selection tuning, not just unification).  Harness scaffolding
  is in `tests/test_atp_ac_bench.c`; add cases as the engine
  unblocks each.
* Lift the per-call CP cap from 64 once a real workload hits it
  (none today; the cap exists so Stickel can't explode on pathological
  inputs).  Larger leaf counts (> 8 per side) bail to the syntactic
  path -- raise via a Diophantine multiplicity basis if a real bench
  needs it.
* Selection / ordering interplay: the AC top-symbol bias in the
  precedence picker (currently AC labels sit at the precedence
  floor) hasn't been re-tuned now that AC-LPO is on; on harder
  problems the wrong AC-vs-skolem layering will starve the
  cracker.

### Additional reduction orderings

Today: KBO, LPO, RPO.  Open items:

* **PO** (Polynomial Ordering) - assigns each symbol a polynomial
  interpretation.  Strong on arithmetic-flavored problems.
* **WPO** (Weighted Path Ordering) - generalizes KBO + LPO + RPO
  via a per-symbol weighted-status combination.
* **RPO flatrec / Vortest port**.  Today's RPO (src/rpo/_.c) uses the
  Term-tree recursion with a per-call memo.  LPO has a flatterm
  Vortest pretest + flatrec recursion (lpo_flat_rec_*) that doubled
  throughput on AndAssoc; the same scaffolding can lift to RPO once
  a real bench shows lpo_flat / rpo_flat hot.
* **RPO under GJ**.  Ground-joinability (`atp_compare`'s gj_less_in
  path) is implemented for KBO only -- under RPO the cracker skips
  GJ entirely.  Lifting gj_less_in to a precedence-only ordering is
  the next gap for RPO-driven completion on hard theories.

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

This is the biggest single arc - it changes the input language.
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

* **Literal selection** - Vampire-style maximal-literal selection
  to reduce inference fan-out.
* **Avatar-style splitting** - propositionally split a clause's
  ground sub-clauses.
* **Inference scheduling** - interleave resolution, paramodulation,
  demodulation per a configurable strategy.

### Saturation algorithms

* **DISCOUNT** - current shape (the given-clause loop with
  passive/active sets).
* **Otter** - explicit `usable` / `sos` queues with different
  selection.  Predates DISCOUNT; partially supported via
  `set_use_sos`.
* **Limited-Resource Strategy** - partially implemented
  (`set_use_lrs`, queue pruning by predicted reachability).
* **Conditional rewriting** - Horn-clause-only reasoning when the
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
