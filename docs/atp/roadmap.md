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
  (`src/rpo`), WPO (`src/wpo`); KBO and LPO with automatic
  precedence (`src/atp/precedence.c`).  RPO adds per-symbol LEX/MUL
  status (LPO is the all-LEX special case; MPO is all-MUL).  WPO
  layers per-symbol integer weights on top of RPO -- weight ties
  fall through to an RPO-style precedence/status comparison.  Wire
  into ATP via `thvm_atp_set_rpo` / `thvm_atp_set_wpo`; dispatch
  priority WPO > RPO > LPO > KBO when multiple are set.
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

Today: KBO, LPO, RPO, WPO.  Open items:

* **PO** (Polynomial Ordering).  Assigns each symbol a polynomial
  interpretation rather than a single integer weight.  Strong on
  arithmetic-flavored problems and the standard companion to KBO
  when a single linear weight doesn't separate the rules.
* **Flatrec / Vortest port to RPO and WPO**.  LPO has a flatterm
  Vortest pretest + flatrec recursion (lpo_flat_rec_*) that
  doubled throughput on AndAssoc; the equivalent scaffolding can
  lift to RPO + WPO once a real bench shows them hot.
* **Ground-joinability under LPO/RPO/WPO**.  `gj_less_in` is
  KBO-only; under non-KBO orderings the cracker skips GJ entirely.
  Lifting it to a precedence-only ordering is the next gap for
  RPO/WPO-driven completion on hard theories.
* **Automatic WPO weight tuning**.  KBO has automatic precedence
  via `src/atp/precedence.c`; WPO needs both weights AND
  precedence picked from the axiom set.  Iterating an LP-style
  weight search (e.g. KBOlin) over the axioms is the standard
  approach.

### Full first-order clauses

`src/fol/_.c` covers:
* Typed `FolLit` / `FolClause` (atoms are `Term` CTRs; sign per
  literal).  Equality atoms use the `FOL_LAB_EQ` (= 0) label.
* Binary resolution (`fol_resolve`) with variable rename-apart via
  `FOL_RENAME_OFFSET` (= `REWRITE_MAX_VAR / 2`).
* Factoring (`fol_factor`) on two same-polarity literals.
* Paramodulation (`fol_paramodulate`) -- superpose a positive
  equality literal into a target subterm at a given path.  Reuses
  `cp_subterm_at` / `cp_replace_at` from `src/cp`.  Both
  orientations via the `swap` flag.
* Reflexivity-resolution (`fol_reflex_resolve`) -- a negative
  equality `¬(s = t)` resolves when s and t unify.
* Multiset-modulo clause equality + empty-clause detection.
* Subsumption (`fol_subsumes`) -- recursive backtracking match
  with C2's variables renamed apart so they act as constants.
* Tautology detection (`fol_is_tautology`) -- catches A v ¬A and
  positive (s = s).
* Equality factoring (`fol_eq_factor`) -- Vampire-style:
  (s1=t1) v (s2=t2) v R with σ unifying s1, s2 yields
  σ((s1=t1) v ¬(t1=t2) v R).  Completes the FOL+equality
  refutation-complete inference family.
* Saturation loop (`CnfState` in src/fol/sat.c) -- Otter-style
  given-clause: FIFO passive queue, active set, per-step
  cross-resolution + paramodulation (BOTH orientations, every
  non-variable position via `cnf_walk_positions` reusing the
  CP_MAX_DEPTH bound from src/cp) + self-factoring + reflex-resolve
  + eq-factoring + self-paramod, tautology + forward AND backward
  subsumption filters on derived clauses.  Deferred-free queue keeps
  mid-step inference loops safe when backward subsumption clobbers
  clauses they captured at entry; queue drains at end-of-step.  No
  indexing yet (O(n^2) subsumption + CP scans); the unit-equational
  layer's discrim-tree machinery can lift in once a workload runs.

The unit-equational saturator in `src/atp/_.c` is unchanged; FOL
clauses live in a parallel module that callers opt into.

Open follow-ups (in order):

* Equality-factoring (positive equalities `s = t` and `u = v` that
  unify on one side -- distinct from the syntactic factoring already
  shipped).
* Selection function (positive/maximal literal-pick policy a la
  E or Vampire).
* Saturation loop with active/passive queues + redundancy:
  forward/backward subsumption, demodulation by equational rules,
  splitting.
* Integration with CNF preprocessing (next section).

### Skolemization + CNF + reflection

`src/fol/cnf.c` covers:
* Reserved CTR labels for connectives (`FOL_LAB_NOT/AND/OR/IMP/IFF/
  ALL/EX`) -- formulas are Term trees, user predicates use labels
  below the reserved range.  thvm's CTR label encoding caps at 18
  bits (262143), so Skolem labels (`FOL_LAB_SKOLEM_BASE = 0x20000`)
  sit above connectives but inside the encoding range.
* `fol_nnf(f)` -- negation normal form: pushes `¬` to atoms,
  eliminates `->` and `<->`, preserves quantifiers.
* `fol_skolemize(f)` -- replaces every `∃y` with `sk_n(x1..xk)` over
  the enclosing `∀`-bound vars; drops surviving `∀` quantifiers.
  Output is quantifier-free with free vars implicitly universal.
  `fol_reset_skolem()` clears the counter between runs.

CNF pipeline closed end-to-end:
* `fol_distribute(f)` -- distribute ∨ over ∧ to reach AND-of-ORs
  shape.  Worst-case exponential in nested-OR depth (the standard
  CNF blowup); Tseitin structural CNF is a follow-up if a real bench
  hits the blowup.
* `fol_extract_clauses(cnf, &n_out)` -- walk the AND-of-ORs tree and
  emit one FolClause* per top-level disjunct.  Negation literals
  decoded from `¬atom` connectives.
* `fol_formula_to_clauses(f, &n_out)` -- one-shot wrapper: NNF +
  Skolem + distribute + extract.  Resets Skolem counter each call.

End-to-end smoke (in tests/test_fol.c):
  `∀x.(P(x) -> Q(x)) ∧ P(a) ∧ ¬Q(a)`  -> PROVED via cnf_run.
  Russell-style `∀x.(R(x,x) <-> ¬R(x,x))`  -> PROVED.

Standard FOL bench (in tests/test_fol_pelletier.c):
  Pelletier P1-P14 (propositional) + P15/P18/P19 (FOL with
  quantifier alternation) all PROVE end-to-end via
  fol_formula_to_clauses -> cnf_run.  Covers contrapositive,
  double-negation, Peirce's law, biconditional associativity,
  distributivity, mutual implications, ∀-distribution, the drinker
  paradox, and ∃-bound nested implications.

Demodulation lands in two layers:

* Primitive `fol_demodulate(eq_clause, target)` in src/fol/_.c:
  a unit positive equality `[s = t]` rewrites every literal atom of
  the target clause via thvm_match + thvm_subst_apply.  Returns a
  fresh clause on change, NULL otherwise.  Rule variables renamed
  apart by FOL_RENAME_OFFSET so target vars stay consistent across
  literals.

* Forward demodulation in `cnf_consider` (src/fol/sat.c): every
  derived clause is normalized against the active set's unit
  positive equalities BEFORE tautology / subsumption checks, up to
  CNF_DEMOD_BUDGET = 16 iterations.  The budget caps cyclic
  rewrite pairs (a=b ∧ b=a) under naïve no-ordering demod.

* Backward demodulation in `cnf_consider` (src/fol/sat.c): when the
  freshly-added clause is itself a unit positive equality, walk
  every existing clause and apply the new rule.  Modified clauses
  get a fresh id with the demodulated content; the old slot is
  NULLed + deferred-free, mirroring `cnf_backward_subsume`'s pattern.

Naïve (no σ(s) > σ(t) ordering check yet); the ordering-aware
variant is a follow-up that plugs in a KboConfig / LpoConfig /
RpoConfig / WpoConfig.

Selection function lands in src/fol/sat.c: `CnfSelection` enum
(NONE / NEGATIVE / POSITIVE) + `cnf_set_select(s, sel)` setter.
When set, cnf_gen_resolution restricts both faces to their selected
literal; cnf_gen_paramod restricts the TARGET face only (source
must remain a positive equality regardless).  Falls back to "all
literals" when the policy finds no matching lit -- preserves
completeness for Horn-style inputs without the target polarity.

Proof reconstruction lands as a per-clause inference trace + DAG
printer:
* `FolInference` enum: INPUT / RESOLVE / FACTOR / PARAMOD / REFLEX
  / EQ_FACTOR / DEMOD.
* `FolTrace` parallel array in CnfState: `{rule, parent_a,
  parent_b}` per clause id.  `FOL_NO_PARENT = 0xFFFFFFFF` sentinel.
* Inference functions in src/fol/sat.c stash the trace via
  cnf_add_clause_traced; user-added input clauses use the existing
  cnf_add_clause path (defaults to INPUT trace).
* `cnf_print_proof(s, FILE*, root_id)` walks the trace DAG from
  any root (typically the empty clause when status == PROVED) and
  prints a text proof: one line per ancestor with rule + parents
  + literal list.  Visited bitmap dedups; parents print before the
  derived clause (post-order on the inverse DAG).

Open follow-ups (efficiency / completeness):
* Tseitin structural CNF for shallow clauses on deeply-nested ∨/∧.
* Discrim-tree / FV-index for fast subsumption + CP queries.
Ordering-aware demodulation lands: CnfState carries optional
KboConfig / LpoConfig / RpoConfig / WpoConfig fields (set via
`cnf_set_{kbo,lpo,rpo,wpo}`).  Demod's per-position rewrite gates
on σ(s) > σ(t) under the configured ordering when any is set,
falling back to naïve unconditional rewriting otherwise.  Dispatch
priority WPO > RPO > LPO > KBO.  Cyclic rule pairs (a=b ∧ b=a) no
longer fire under a well-founded ordering -- the CNF_DEMOD_BUDGET
cap is only relevant for the naïve fallback.

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
