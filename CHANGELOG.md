# Changelog

Human-readable log of meaningful changes. Newest first. Group entries
under `## Unreleased` until we cut a tagged version, then roll into a
dated section.

## Unreleased

### Added: SUP-of-rules dispatch demo (stage 8.3c)

`tests/test_sup_rewrite.c` (23 sub-checks, 4 cases) demonstrates
that a SUP of `prim_rewrite_step` calls reduces to the same
rewrite outcomes as direct C-side `thvm_match` +
`thvm_subst_apply` calls would produce per rule.

Cases:
- `sup-rewrite/two-rules-one-applies`: rules `f(x, e) -> x`
  and `g(x) -> a`; target `f(b, e)`.  Rule 0 matches yielding
  `b`; rule 1 yields ERA.  SUP children's wnf outputs match
  direct C-side reference per rule.
- `sup-rewrite/two-rules-both-apply`: bare-FVR rules
  `x -> a` and `x -> b`.  Both match anything; outcomes are
  the rules' RHS unchanged.
- `sup-rewrite/three-rules-mixed`: nested SUP `&L_outer{r0,
  &L_inner{r1, r2}}` covers >2 rules.  Ref-checked per
  branch.
- `sup-rewrite/app-sup-fan-out-with-num-args`: exercises
  APP-SUP commutation directly with NUM args (DUP-friendly)
  to confirm the fan-out machinery operates with PRI children.

CP-shaped tests use the "fully-applied PRI inside the SUP"
encoding (`&L{APP(APP(APP(PRI(REWRITE), lhs_i), rhs_i),
target), ...}`) because APP-SUP cannot fan out CTR-shaped
args today (DUP-CTR not yet implemented).  The NUM test
confirms APP-SUP itself works on DUP-friendly args.  Caveat
documented in the test header; mirrors 8.1d-ii's pattern.

### Added: prim_rewrite_step IC dispatch primitive (stage 8.3b)

`prim_rewrite_step` (arity 3) registered at
`ATP_PRIM_REWRITE_STEP = 4` during `thvm_atp_init`.  Per
`docs/plans/ic_rule_dispatch.md`'s Strategy B: takes
`(lhs, rhs, target)`; runs `thvm_match`; on success returns
`thvm_subst_apply(rhs, &subst)`; on failure returns ERA.
Equivalent to one step of `thvm_rewrite_step` at the top
position, dispatched via APP-PRI evaluation.

`tests/test_rewrite_pri.c` (16 sub-checks, 6 cases):
- `rewrite-pri/direct-match`: `f(x, e) -> x` applied to
  `f(a, e)` -> CTR `a`
- `rewrite-pri/no-match-different-head`: target `g(a)` against
  `f(_, _)` LHS -> ERA
- `rewrite-pri/no-match-second-arg-mismatch`: target `f(a, b)`
  against `f(_, e)` LHS -> ERA
- `rewrite-pri/fvr-only-lhs-binds-anything`: bare `x` LHS
  matches any term
- `rewrite-pri/nested-ctr-binds-multiple-vars`:
  `f(g(x), y) -> g(y)` on `f(g(a), b)` -> `g(b)`
- `rewrite-pri/repeated-var-must-match-consistently`:
  `f(x, x)` matches `f(a, a)` but not `f(a, b)`

Combined with APP-SUP fan-out (8.3c) this lets a SUP of
partial-PRI rules dispatch in parallel against a single target
term.

### Added: IC-native rule dispatch design memo (stage 8.3a)

`docs/plans/ic_rule_dispatch.md` (~150 lines) lays out the
design for stage 8.3.  Identifies the **FVR-vs-VAR translation
problem**: our pattern variables are `TAG_FVR` atoms with
explicit ids, but `LAM` uses `TAG_VAR` binder slots that point
at heap cells.  Surveys three encoding strategies:

1. **Strategy A (literal LAM port)**: alpha-convert FVR to VAR
   via per-id binder cells, wrap in LAM chain.  Fails for nested
   patterns since the IC reducer doesn't naturally peel nested
   CTR.
2. **Strategy B (PRI dispatch)**: keep FVR, register a
   `prim_rewrite_step` (arity 3) at `ATP_PRIM_REWRITE_STEP = 4`
   that does `thvm_match` + `thvm_subst_apply`; SUP of partial
   PRIs handles fan-out via APP-SUP.  Faithful, simple, parity
   story is clear.
3. **Strategy C (hybrid)**: LAM only on outermost args; defer
   nested patterns to a primitive.  Fiddly, unclear win.

**Decision**: Strategy B for 8.3b-c.  Reinterprets the literal
"rule as LAM-binder" phrasing as "rule as a callable IC entity"
-- the LAM-vs-PRI choice is a means (IC dispatch), not an end.

8.3d (ICC TAG_BRI / TAG_ANN integration) deferred until 8.4
lands multi-sort signatures.  Without sorts, every rule applies
to any term and BRI/ANN wrapping is ceremony.

8.3e mirrors 8.1e: feature flag `use_ic_rewrite` swap of
`thvm_rewrite_step` under bench harness validation.

### Added: pure-IC kbo_eq via prim_kbo_eq_ic (stage 8.2c)

`prim_kbo_eq_ic` (arity 2) registered at `ATP_PRIM_KBO_EQ_IC = 3`
during `thvm_atp_init`.  Returns `NUM(0)` or `NUM(1)` depending
on structural equality of the two argument terms.  Implementation
splits on tag:

- Tag mismatch / ext mismatch -> immediate `NUM(0)`
- `TAG_FVR` with same ext -> `NUM(1)` (same variable id)
- `TAG_CTR` with arity 0 -> `NUM(1)`
- `TAG_CTR` with arity n > 0 -> **builds** an AND chain of n
  self-recursive APP-PRI calls and returns the unfired chain.
  The wnf reducer then evaluates the AND, firing each child
  comparison through APP-PRI saturation, short-circuiting on
  the first NUM(0).
- Other tags -> compare `term_val` directly

This is "IC-driven structural recursion with C base cases" --
the design memo's option (2) at minimum scope, demonstrating
that recursive structural code runs through our reducer
end-to-end.

`tests/test_kbo_pri.c` adds 9 cases (35 sub-checks total, was
17): leaf FVR same-id / different-id / tag mismatch, nullary
CTR same-label / different-label, binary CTR (equal, first
child differs, second child differs), nested CTR 3-level
recursion.

Stage 8.2c closes; 8.2d (full pure-IC port of `thvm_kbo`)
remains deferred until SupGen-style search (8.10) creates a
concrete use case.

### Added: thvm_kbo as TAG_PRI primitive (stage 8.2b)

`prim_kbo` (arity 3) registered at `ATP_PRIM_KBO = 2` during
`thvm_atp_init`.  Takes `(s, t, cfg_id_NUM)`; resolves cfg_id
via the new process-global `KBO_CFG_TABLE` (cap 16); calls
`thvm_kbo(s, t, cfg)`; returns `NUM(KboCmp)` (0=EQ, 1=GT, 2=LT,
3=UN per the existing enum), or `ERA` if cfg_id is bogus / no
config registered / cid arg is not a NUM.

New API in `src/thvm.h`:
- `u32 kbo_cfg_register(u32 cfg_id, const KboConfig *cfg);`
- `const KboConfig *kbo_cfg_get(u32 cfg_id);`
- `#define KBO_CFG_TABLE_CAP 16`
- `#define ATP_PRIM_KBO 2u`

`tests/test_kbo_pri.c` (17 sub-checks, 6 cases):
- `kbo-pri/registry-roundtrip`: register/get + out-of-range
- `kbo-pri/eq-outcome`: identical terms -> NUM(KBO_EQ),
  parity-checked vs direct C call
- `kbo-pri/gt-outcome`: `f(x, e) > x` -> NUM(KBO_GT)
- `kbo-pri/lt-outcome`: mirror image -> NUM(KBO_LT)
- `kbo-pri/un-outcome`: `f(x, y)` vs `f(y, x)` -> NUM(KBO_UN)
- `kbo-pri/unregistered-cfg-falls-through-to-ERA`: bogus cfg_id

This unblocks 8.10 (SupGen-style search) to invoke KBO from
inside an APP-PRI evaluation chain -- the minimum useful
increment per `docs/plans/kbo_ic_design.md`.

### Added: KBO-as-IC encoding design memo (stage 8.2a)

`docs/plans/kbo_ic_design.md` (~150 lines) lands the design
sketch for stage 8.2.  Surveys three encoding options:

1. **TAG_PRI wrapper** (~50 LOC): registers `thvm_kbo` as a
   primitive callable from IC; mirrors 8.1c's
   `prim_unify_apply`.  Unblocks 8.10 to invoke KBO from inside
   a SUP-encoded search.  Minimum useful increment.
2. **Hybrid IC structural recursion + C arithmetic primitives**
   (~200 LOC): structural recursion in IC, weights and counts in
   C as TAG_PRI callbacks.  Lets 8.10 superpose alternative
   ordering structures without porting arithmetic to IC.
3. **Full pure IC** (~500-1000 LOC): everything in IC, including
   Church-numeral or TAG_NUM weights and IC-encoded variable
   counts.  Research target; lets 8.10 superpose KboConfigs
   themselves.

Decision:
- 8.2b implements (1) immediately -- bounded scope, ports cleanly
  from `prim_unify_apply`.
- 8.2c implements a sliver of (2): pure-IC `kbo_eq` (the
  structural-equality sub-routine) as a proof point that IC-
  driven recursion is viable in our codebase.
- 8.2d (full pure IC) deferred until SupGen-style search (8.10)
  materializes and creates a concrete use case that pays for
  the engineering cost.

The memo also documents:
- The KboConfig registry pattern: `KBO_CFG_TABLE[16]` keyed by
  a u32 id, since `KboConfig*` doesn't fit cleanly in a Term's
  `val` field.
- An `prim_kbo` sketch (arity 3: `(s, t, cfg_id_NUM)` -> NUM
  encoding of `KboCmp`).
- Parity-test verification plans for each subtask.

### Added: IC vs C path bench comparison (stage 8.1e-iii)

`tests/test_bench_atp.c` now runs each `.pr` fixture under both
CP-gen modes (C-direct + IC-routed) and emits one CSV row per
`(file, mode)` pair into `build/bench-atp.csv`.  New `mode`
column joins the existing schema.

Results on the 4-fixture corpus, darwin/arm64, single run:

- All counters (step, n_rules, n_trace, all four
  `n_cps_dropped_*`) are **byte-identical** between C and IC
  paths.  Empirically confirms 8.1e-ii's parity claim.
- Wall-clock: IC is within run-to-run noise of C on every
  fixture.  On the TIMEOUT case (`group_commutative_inverse.pr`)
  C=132 ms, IC=119 ms; both well within the 2x target.

Hypothesis: CP-gen time is dominated by the position walk +
unification itself; the IC wrapper (APP-PRI accumulation, wnf
reduction) adds only a small constant per call.  σ is
recomputed twice per CP through `prim_unify_apply3`, which is
wasteful but cheap on small problems.

**Decision**: `use_ic_cp_gen` default stays off (C path is more-
tested).  IC is production-viable opt-in for SupGen-style search
(8.10).  If larger TPTP-UEQ problems show >2x slowdown, the
single-σ primitive idea (return CTR-pair of (σ(replaced),
σ(ri))) is the obvious mitigation.

`docs/bench-atp.md` updated with the full comparison table and
analysis under a new "IC path vs C path" subsection.

Stage 8.1 -- SUP-encoded CP enumeration via TAG_PRI unify --
is now complete.

### Added: IC-routed CP enumeration (stage 8.1e-ii)

`thvm_atp_generate_cps_ic` now actually routes the per-position
unify+apply step through the TAG_PRI machinery instead of
delegating to the C path:

- New primitive `prim_unify_apply3` (arity 3) registered at id
  `ATP_PRIM_UNIFY_APPLY3 = 1`.  Takes `(s, t, target)`; returns
  `thvm_unify_apply(target, &σ)` where `σ = mgu(s, t)`, or `ERA`
  on unify failure.
- New helper `ic_unify_apply3(s, t, target)` builds the saturated
  APP chain `APP(APP(APP(PRI(1), s), t), target)` and reduces it
  via `wnf`.  Each invocation flows through APP-PRI accumulation
  and saturated-call dispatch.
- New visitor `cp_visit_ic` (mirrors `cp_visit` from
  `src/cp/_.c`) routes both `σ(replaced)` and `σ(ri)` calls
  through `ic_unify_apply3`.  Recomputes σ once per side
  (wasteful but correct -- 8.1e-iii will measure).
- `thvm_atp_generate_cps_ic` reuses the C-side
  `cp_walk_positions` for the (i, j, position) enumeration but
  feeds it the IC-routed visitor.

Same iteration pattern as the C path; structurally identical
output verified by parity tests.

`tests/test_atp.c` (8378 sub-checks, was 8376):
- `cp-gen-flag-toggle-preserves-output` updated to assert the
  IC path now actually runs (not delegating)
- `cp-gen-ic-parity-on-group-axioms` new: full saturation on
  the group axioms under both paths must agree on rst and
  n_rules

Stage 8.1e-iii will benchmark the IC overhead and decide on
the default.

### Added: `use_ic_cp_gen` feature flag (stage 8.1e-i)

New `u8 use_ic_cp_gen` field on `AtpState` (default 0) selects
between the C-direct and IC-routed critical-pair enumerators.
`thvm_atp_generate_cps` now dispatches:

- `use_ic_cp_gen == 0`: `thvm_atp_generate_cps_c` (renamed body
  of the previous implementation; the C-direct path)
- `use_ic_cp_gen == 1`: `thvm_atp_generate_cps_ic` (currently a
  no-op wrapper that delegates to the C path; 8.1e-ii will land
  the actual SUP+PRI routing)

Tests in `tests/test_atp.c`:
- `atp/cp-gen-flag-default-off`: fresh AtpState has flag 0
- `atp/cp-gen-flag-toggle-preserves-output`: enabling the flag
  must produce identical n_cps / n_rules / n_cps_dropped_joinable
  to the default path on the same input (since the IC path is
  currently a delegate)

`tests/test_atp.c`: 8376 sub-checks (was 8370).  Stage 8.1e-ii
will replace the IC path's body with PRI-routed unification.

### Added: SUP-encoded CP fan-out demo (stage 8.1d-ii)

`tests/test_sup_cps.c` (21 sub-checks, 4 cases) demonstrates that
a SUP of `prim_unify_apply` calls reduces to the same terms as
direct C-side `thvm_unify_apply` would produce for each pair --
the structural parity check from `docs/plans/sup_encoded_cps.md`:

- `sup-cps/two-positions-both-unify`: `&L{(f(x), f(a)),
  (g(y), g(b))}` -- both branches unify; child wnfs match
  reference `thvm_unify_apply` outputs term-by-term
- `sup-cps/one-unifies-one-fails`: mixed -- one branch yields
  CTR, the other ERA
- `sup-cps/three-positions-mixed`: nested SUP `&L_outer{p1,
  &L_inner{p2, p3}}` -- demonstrates the encoding scales beyond
  binary
- `sup-cps/app-sup-fan-out-with-num-args`: exercises APP-SUP
  commutation directly with NUM args (DUP-NUM is implemented;
  DUP-CTR isn't yet) -- a `&L{PRI(40), PRI(40)}` applied to
  NUM(11) fans out and each branch's identity primitive
  returns 11

The CP-shaped tests use the "fully-applied PRI inside the SUP"
encoding (`&L{APP(APP(PRI_unify, s_i), t_i), ...}`) because
APP-SUP cannot fan out CTR-shaped args today (no DUP-CTR).
The 4th test confirms APP-SUP itself works on DUP-friendly args.

Stage 8.1d closes; the design memo's parity claim is now
empirically verified at the 2-3 position scope.

### Added: APP-SUP commutation (stage 8.1d-i)

`src/interact/app_sup.c` lands the standard HVM4 rule:

```
APP(&L{f, g}, arg)
------------------ APP-SUP
&L{ APP(f, arg_0), APP(g, arg_1) }   where ! &L{arg_0, arg_1} = arg
```

Allocates a 7-cell block: shared DUP body for `arg`, two APP slots
referencing the DUP via DP0/DP1, two SUP children pointing at the
APPs.  Wired into the WNF dispatch in `src/wnf/_.c` and
`src/wnf/redex.c` next to APP-LAM / APP-BRI / APP-PRI.

Foundational interaction; not 8.1-specific but blocks 8.1d-ii
(SUP-encoded CP fan-out).

`tests/test_app_sup.c` (16 sub-checks, 5 cases): single-fanout
with PRI children, label preservation, asymmetric children
(PRI vs ERA), ERA arg fan-out, ITRS counter increment.

Caveat documented in the test header: APP-SUP shares the arg
via a DUP, which fires only for tags with DUP-* interactions.
Today our IC has `DUP-{ERA, LAM, NUM, SUP, BRI, ANY}`; CTR and
FVR remain passive.  8.1d-ii will route CP enumeration around
this (pass the rule pair through the SUP rather than as the
APP arg) or land DUP-CTR as a separate task.

### Added: ATP unification as a TAG_PRI primitive (stage 8.1c)

`src/atp/_.c` registers `prim_unify_apply` (arity 2) at id
`ATP_PRIM_UNIFY_APPLY = 0` during `thvm_atp_init`.  The
primitive takes two terms `(s, t)`, runs `thvm_unify`; on
success returns `thvm_unify_apply(s, &subst)` (the unified
term), on failure returns `ERA` so the surrounding APP-PRI
structure short-circuits cleanly via APP-ERA when consumed by
SUP-encoded CP enumeration in 8.1d.

`tests/test_pri.c` adds 3 round-trip cases (now 25 sub-checks
total):
- `pri/unify-apply/var-ctr`: `(f(x), f(a))` -> `f(a)`
- `pri/unify-apply/incompatible-ctrs-give-ERA`:
  `(f(x), g(y))` -> `ERA`
- `pri/unify-apply/identical-vars-trivial-success`:
  `(x, x)` -> `x`

`atp_register_primitives` is idempotent (registry overwrites
with the same fn pointer), so multiple `thvm_atp_init` calls
do not double-register.

### Added: TAG_PRI primitive function call (stage 8.1b)

New IC tag `TAG_PRI = 25` (HVM4 port) lands a "primitive function
call" mechanism: a PRI carries a `prim_id` (u32, in EXT) into a
process-global registry mapping id -> `(PrimFn, arity)`.  APP-PRI
accumulates args into a heap cell `[NUM(count), arg_0, ...]` until
`count == arity`, at which point the registered C function is
called and its return Term replaces the redex.

New API in `src/thvm.h`:
- `typedef Term (*PrimFn)(Term *args);`
- `Term term_new_pri(u32 prim_id);`
- `u32 prim_register(u32 prim_id, PrimFn func, u32 arity);`
- `PrimFn prim_fun(u32 prim_id);`
- `u32 prim_arity(u32 prim_id);`
- `#define PRIM_TABLE_CAP 64`

`TAG_COUNT` bumped to 26.  `tests/test_tensor.c` updated.

New files:
- `src/term/new_pri.c` -- constructor + registry storage
- `src/interact/app_pri.c` -- APP-PRI accumulation + saturation

WNF dispatch wired in `src/wnf/_.c` and `src/wnf/redex.c`.

`tests/test_pri.c` (18 sub-checks) covers:
- Tag + ext layout on a fresh PRI
- Registry roundtrip + out-of-range cleanup
- Arity-1 immediate fire (identity)
- Arity-2 partial-then-saturate (pair-CTR builder)
- Arity-3 saturation across 3 APPs
- Unregistered prim_id falls through to ERA defensively

Stage 8.1c will register `thvm_unify` as the first real primitive.

### Added: SUP-encoded CP enumeration design memo (stage 8.1a)

`docs/plans/sup_encoded_cps.md` (~200 lines) lands the design
sketch for stage 8.1.  Surveys HVM4's `TAG_PRI` reference
implementation (registry table mapping `id -> (PrimFn, arity)`,
APP-PRI partial-application interaction); spells out the
SUP-cross-product encoding for the
`outer_rule x inner_rule x overlap_position` triple via three
labeled SUPs and APP-SUP commutation; analyzes feasibility
and concludes labeled SUPs suffice (8.6 unordered SUPs are an
optimization, not a prerequisite).

Migration target documented: `thvm_unify`, `thvm_match`,
`thvm_subst_apply`, `kbo_eq` stay in C as `TAG_PRI` callbacks;
`thvm_critical_pairs_range` and `atp_push_cps_traced`'s loop
move to IC.

Decision: 8.1 unblocks 8.10 (SupGen-style search) -- 8.10
needs CPs reified as SUP entries to superpose the
"which next CP" choice.  Implementation order: 8.1 then 8.10.

Stop conditions for the implementation subtasks: revert to
8.4 / 8.5 if `TAG_PRI` integration runs into the existing
stack-machine reducer; treat 8.1 as research infrastructure
(not a perf win) if IC enumeration is structurally correct
but asymptotically slower.

### Added: Twee comparison harness `tools/bench_twee.c` (stage 7.4d)

`tools/bench_twee.c` parses each `.pr` in `tests/data/atp/` via
`wald_parse_file`, emits a TPTP-CNF representation
(`cnf(eqn<i>, axiom, lhs = rhs).` for axioms,
`cnf(goal, negated_conjecture, lhs != rhs).` for the goal) into
`build/bench_twee_<i>.tptp`, then invokes
`twee --quiet --no-proof --max-cps 256 <tptp>` matching our own
budget.  Wall-clock + Twee's status are written to
`build/bench-twee.csv`.

`make bench-twee` builds and runs the comparison; not part of
`make test` (Twee is an external dependency, install via
`cabal install twee`).

Twee 2.6.1 was successfully installed via `cabal install twee`
on this host (darwin/arm64 + GHC 9.12.1; cabal warned about
GHC version but compile succeeded for all 20 transitive
dependencies including `twee-lib`, `jukebox`, `minisat`).

First comparison numbers, 2026-04-26:

| File | Twee | thvm |
|---|---|---|
| `group_commutative_inverse.pr` | PROVED 26.1 ms | TIMEOUT 132.7 ms |
| `group_right_inverse_to_e.pr` | PROVED 26.7 ms | PROVED 0.006 ms |
| `idempotent_nested.pr` | PROVED 24.2 ms | PROVED 0.001 ms |
| `monoid_right_id.pr` | PROVED 30.0 ms | PROVED 0.001 ms |

Twee proves all four; we prove three out of four.  The harder
group-commutativity goal isolates a real gap (LPO + better
heuristic).  For the easy goals our IC-native ATP wins on
latency by 3-4 orders of magnitude (no process spawn, no TPTP
parse, no warm-up), but Twee wins on hard-saturation
throughput.  `docs/bench-atp.md` records the full table and
observations.

Stage 7.4 complete; stage 7 (Twee-class redundancy criteria
and benchmarking) closed.

### Added: ATP bench harness `test_bench_atp` (stage 7.4c)

`tests/test_bench_atp.c` walks `tests/data/atp/*.pr`, runs our
ATP on each with a fixed step budget (256), times via
`clock_gettime(CLOCK_MONOTONIC)`, and writes per-file rows to
`build/bench-atp.csv` with columns:
`file,status,wall_ms,step,n_rules,n_trace,drop_joinable,
drop_connected,drop_rule_subsumed,drop_queue_subsumed`.

Soft regression: only the final ATP `status` is asserted against
the matching `.expect` file (so PROVED <-> TIMEOUT swaps fail
the test); step / rule / counter values are recorded but not
gated -- the bench is a measurement, not a regression.

Wired into the standard `TESTS` list so `make test` rebuilds and
runs it; `make` exit code stays 0 on numeric drift.  CSV is
regenerated on every run.

`docs/bench-atp.md` now points at the harness for re-runs and
records the full 4-row results table sourced from
`build/bench-atp.csv`.

### Added: `.pr` test corpus for ATP bench (stage 7.4b)

Four small group-flavored `.pr` fixtures land under
`tests/data/atp/`, each paired with a `.expect` companion that
records the empirically-observed outcome (status + advisory
upper bounds on step / rule count):

- `group_right_inverse_to_e.pr` -- group axioms, conclude
  `f(a, i(a)) = e` (direct rewrite). Expected: PROVED in 1
  step, 2 rules.
- `group_commutative_inverse.pr` -- group axioms, conclude
  `f(a, i(a)) = f(i(a), a)` (commutativity-of-inverse-on-
  element; same as `waldmeister/documents/example.pr`).
  Expected: TIMEOUT at 256 steps under the current KBO config.
- `monoid_right_id.pr` -- monoid (assoc + right-id, no
  inverse), conclude `f(a, e) = a`. Expected: PROVED in 0
  steps (closes via direct goal-rewrite).
- `idempotent_nested.pr` -- pure idempotent rule
  `f(x, x) = x`, conclude `f(a, f(a, a)) = a`. Expected:
  PROVED in 0 steps via the recursive rewriter.

`.expect` format: simple `key=value` pairs with `%`-prefixed
comments (matching the `.pr` lexer's syntax).  Recognized keys:
`status`, `max_step`, `max_rules`. Stage 7.4c will add a bench
harness that consumes these.

### Added: ATP benchmark log skeleton (stage 7.4a)

`docs/bench-atp.md` lands the methodology + first results table
for our IC-native ATP. Records 8 metrics per run (status,
wall-clock ms, saturation step count, rule-set size, trace
length, and the four `n_cps_dropped_*` counters from 7.1/7.2b/
7.3a/7.3b) on two starting cases:

| File | Status | Wall (ms) | Steps |
|---|---|---|---|
| simple goal `f(a, i(a)) = e` | PROVED | 0.007 | 1 |
| `waldmeister/documents/example.pr` (`f(a, i(a)) = f(i(a), a)`) | TIMEOUT | 130.765 | 256 |

Confirms (a) the simple goal closes in step 1 via direct rewrite,
(b) the harder commutativity-of-inverse goal needs more than 256
steps under the current KBO config (~52% of generated CPs are
trivially joinable; 231 rules accumulated without proving),
(c) the domination invariants from 7.2b / 7.3a hold empirically
(`drop_connected` 697 <= `drop_joinable` 743;
`drop_rule_subsumed` 212 <= 743), and (d) 7.3b's queue-
subsumption fires rarely (2 hits) on the group example.

Twee comparison deferred to 7.4d (Twee not installed locally).
Bench harness deferred to 7.4c.

### Added: queue-subsumption filter (stage 7.3b)

`src/atp/_.c` gains `atp_cp_queue_subsumed(s, lhs, rhs)`: returns 1
if the candidate `(lhs, rhs)` is a substitution instance of some
already-queued CP `(s->cp_lhs[k], s->cp_rhs[k])` -- i.e., there is
σ such that `(lhs, rhs) = (σs', σt')` (forward) or `(σt', σs')`
(symmetric).

Genuinely orthogonal to 7.1: the queue does not participate in
`thvm_rewrite_normalize`, so the queue-subsumption check can fire
on CPs that 7.1 misses (and vice versa).  Wired as a real FILTER
in `atp_push_cps_traced`: candidate is dropped, `n_cps_dropped_
queue_subsumed` ticks, queue does not grow.

`tests/test_atp.c` adds 5 cases:
- `cp-queue-subsumed-direct-instance`: forward direction fires
- `cp-queue-subsumed-symmetric-instance`: symmetric direction fires
- `cp-queue-subsumed-empty-queue-no-fire`: nothing to subsume
  against
- `cp-queue-subsumed-non-instance-no-fire`: non-instance does not
  fire
- `cp-queue-subsumed-filter-drops-instance`: end-to-end filter
  test via `atp_push_cps_traced`

Stage 7.3 is now complete.

### Added: rule-subsumption counter (stage 7.3a)

`src/atp/_.c` gains `atp_cp_rule_subsumed(s, lhs, rhs)`: returns 1
if there exist `(l, r) ∈ R` and substitution σ such that
`(lhs, rhs) = (σl, σr)` (forward) or `(σr, σl)` (symmetric).
Equational subsumption: σ is consistent across both sides
(extended through both `thvm_match` calls on the same
`RewriteSubst`).

Per the same domination argument as 7.2b: rule-subsumption fires
only when an existing rule rewrites lhs to rhs in one step under σ,
which 7.1's full-R normalize also catches.  The counter
`n_cps_dropped_rule_subsumed` ticks unconditionally for empirical
measurement and is bounded above by `n_cps_dropped_joinable`.

`tests/test_atp.c` adds 4 cases:
- `cp-rule-subsumed-direct-instance`: forward direction fires
- `cp-rule-subsumed-symmetric-instance`: symmetric direction fires
- `cp-rule-subsumed-non-instance-no-fire`: non-instance does not fire
- `cp-rule-subsumed-domination-on-saturation`: invariant holds on the
  group example

Stage 7.3b will add queue subsumption -- which IS orthogonal to
7.1 and adds genuine new pruning.

### Added: source-rule-disjoint connectedness counter (stage 7.2b)

`src/atp/_.c` gains `atp_cp_source_disjoint_connected(s, lhs, rhs,
rule_a, rule_b)`: returns 1 if `(lhs, rhs)` is joinable under
`R \ {rule_a, rule_b}` (the two rules that birthed the CP via
overlap unification).  Implementation: builds a filtered rule
array excluding `rule_a` and `rule_b`, normalizes both sides
under it, compares via `kbo_eq`.

`atp_push_cps_traced` signature extended with `(rule_a, rule_b)`;
calls 7.2b's check alongside 7.1's.  New `n_cps_dropped_connected`
field on `AtpState` ticks unconditionally for measurement (does
not gate dropping -- that remains 7.1's job).  Per the domination
lemma in `connectedness_design.md`, the connected count is bounded
above by the joinable count.

Sentinel: passing `ATP_MAX_RULES` for either `rule_a` or `rule_b`
means "exclude no rule," making the function fall through to
trivial-joinability semantics.

`tests/test_atp.c` adds 4 cases (all in the same TEST_BEGIN
group as 7.1's filter tests):
- `cp-connectedness-counter-on-self-overlap`: domination
  invariant holds on self-overlap
- `cp-connectedness-genuine-CP-not-dropped`: hand-constructed
  CP `(a, e)` survives both filters when neither parent rule
  helps the join
- `cp-connectedness-empty-filter-falls-through`: sentinel
  exclusion makes the connectedness check equivalent to
  trivial-joinability
- `cp-connectedness-domination-on-saturation`: empirical
  confirmation on the group example: connected count <= joinable
  count throughout

Stage 7.2 is now complete.

### Added: connectedness redundancy design memo (stage 7.2a)

`docs/plans/connectedness_design.md` (~150 lines): surveys three
candidate Bachmair-Dershowitz-Plaisted (BDP) connectedness
criteria -- subsumption-connected, source-rule-disjoint connected,
"connected below c" -- and proves a *domination lemma*: any rule
subset `R' ⊆ R` cannot find joins that `R` itself cannot find, so
all three candidates are strictly dominated by 7.1's trivial-
joinability filter (which uses full R).

Decision: implement criterion (2), source-rule-disjoint
connectedness, in 7.2b *as an empirical demonstration* of the
domination relationship rather than as a new pruning mechanism.
The resulting `n_cps_dropped_connected` counter is expected to be
a strict lower bound on `n_cps_dropped_joinable`; this is useful
infrastructure for stage 7.4+ when AC theories or extended
joinability tests can break the domination.

The memo also recommends prioritizing stage 7.3 (subsumption)
since it is genuinely orthogonal to 7.1 -- subsumption can fire
on CPs that are not joinable, e.g. instances of an existing rule
that have not been reduced.

### Added: trivial-joinability CP filter (stage 7.1)

Drops critical pairs that are joinable-by-current-R at generate time
rather than letting them flow through the queue and orient pipeline.
This is the simplest version of Waldmeister's `Grundzusammenfuehrung`
("ground-merging") criterion -- equivalent to Twee's
"joinable-by-current-R" pruning.

Implementation:
- `static u8 atp_cp_trivially_joinable(s, lhs, rhs)` in
  `src/atp/_.c` -- normalizes both sides under R via
  `thvm_rewrite_normalize` (NORM_CAP=64) and returns
  `kbo_eq(l, r)`.
- `atp_push_cps_traced` calls it before pushing; on hit, bumps
  `n_cps_dropped_joinable` and skips both the trace push and the
  queue push.
- New `u32 n_cps_dropped_joinable` field in `AtpState` records
  the count for benchmarking.

Behavior change: a single rule's self-overlap CP is always
trivially joinable, so `generate_cps` now returns 0 in that case
(previously it pushed the CP to the queue, where step would drop
it after popping).  Updated tests:
- `atp/generate-cps-single-rule-self-overlap`: pushed == 0, counter
  ticks
- `atp/generate-cps-old-times-new-direction`: pushed == 0, counter
  ticks (assoc + left-id overlaps are all joinable)
- `atp/trace-cp-records-source-rules-as-parents`: rebuilt with two
  non-confluent rules so a non-joinable CP survives
- `atp/trace-serialize-orient-with-parent`: drops the now-absent
  "(cp from N, M):" assertion

New test cases:
- `atp/cp-joinability-filter-self-overlap-counter`
- `atp/cp-joinability-filter-survives-non-joinable`
- `atp/cp-joinability-filter-counter-on-saturation`

Stronger criteria (ground-joinability over a sample of
substitutions, AC-aware joinability) are deferred to 7.2+.

### Added: PCL DAG well-formedness cross-check (stage 6.4c)

`tests/test_wald.c` adds `wald/example.pr/pcl-dag-well-formed`,
which structurally cross-checks our trace against Waldmeister's
PCL format (sources/INF/pcl.c).

Documented format mapping:
- `tes-eqn  : <l> = <r> : initial`             <-> our `(axiom)`
- `tes-rule : <l> -> <r> : orient(<src>,<d>)`  <-> our `(orient from N)`
- `tes-eqn  : <l> = <r> : cp(<a>,<pa>,<b>,<pb>)` <-> our `(cp from A, B)`

Known gaps (deferred):
- We render `tes-rule` with `=` rather than `->`
- We don't carry CP overlap positions in the trace
- No `tes-final` line on proof close (implicit in run-status)

What we DO match (verified by walking `atp->trace[]` directly):
- First `n_eqns` trace entries are `TRACE_AXIOM` with no parents
- Every subsequent orient/cp entry has every parent id strictly
  less than its own id (DAG well-formed -- Waldmeister relies on
  the same invariant for PCL replay)
- Total entry count = axioms + orients + cps (unaccounted reasons
  fail the test)

`tests/test_wald.c`: 2420 sub-checks.

### Added: end-to-end .pr -> saturation -> PCL trace test (stage 6.4b)

`tests/test_wald.c` adds a `wald/example.pr/end-to-end-pcl-trace`
case (248 sub-checks total) that closes the loop:

1. `wald_parse_file("waldmeister/documents/example.pr", spec)`
2. Build `KboConfig` from `spec->symbols[i].prec_rank`
3. `thvm_atp_init`, push 3 axioms, set goal `f(a, i(a)) = f(i(a), a)`
4. `thvm_atp_run` (256-step budget)
5. `thvm_atp_trace_serialize` into 8 KB buffer

Structural assertions (hold whether the goal is proved or the
budget is exhausted -- the example.pr conclusion needs left-inverse
derived from right-inverse + associativity + identity, which is
beyond what we can guarantee in a small budget):
- `n_trace >= n_eqns` (each axiom is recorded)
- trace text contains "0 (axiom): ", "1 (axiom): ", "2 (axiom): "
- at least one "(orient from " line exists
- final `AtpStatus` is one of PROVED / TIMEOUT / QUEUE_EMPTY

The test silently passes if the `waldmeister/` symlink isn't
present (matches the 6.4a fallback).

### Added: `wald_parse_file` -- file-loader convenience wrapper (stage 6.4a)

Thin wrapper in `src/wald/_.c`: opens `path`, slurps the bytes,
calls `wald_parse`, frees the buffer.  New error code
`WALD_ERR_FILE = 3` covers open/read/alloc failure.

API: `WaldErr wald_parse_file(const char *path, WaldSpec *spec)`.

`tests/test_wald.c` adds 4 cases (236 sub-checks total):
- `wald/parse-file/null-path` -> `WALD_ERR_NULL`
- `wald/parse-file/null-spec` -> `WALD_ERR_NULL`
- `wald/parse-file/missing-file` -> `WALD_ERR_FILE`
- `wald/parse-file/example.pr-from-disk` -- loads
  `waldmeister/documents/example.pr` via the vendored-tree
  symlink.  Asserts spec identity (`name == "group"`,
  `mode_proof == 1`, 4 symbols, 3 vars, 3 axioms, goal
  populated).  If the symlink is absent (`WALD_ERR_FILE`),
  the test still passes -- this is a research fixture, not a
  regression test.

### Added: Waldmeister .pr parser feeds saturation end-to-end (stage 6.3g)

Two new test cases in `tests/test_wald.c` close the loop between the
`.pr` parser (stages 6.3a-f) and the saturation engine + KBO comparator:

- `wald/parsed-axioms-kbo-orient-correctly` -- parses the full group
  spec, builds a `KboConfig` from `spec->symbols[i].prec_rank` (with a
  +1 shift so `prec_rank == 0` still gets a positive precedence), and
  asserts each of the 3 parsed axioms orients `KBO_GT` under that
  config.
- `wald/parsed-spec-feeds-saturation-and-proves` -- end-to-end pipeline:
  parse `.pr` source, build `KboConfig` from parsed precedences,
  `thvm_atp_init`, push parsed axioms via `thvm_atp_add_equation`,
  `thvm_atp_set_goal` from parsed conclusion, `thvm_atp_run`, and
  assert `ATP_PROVED` within a small step budget.

This verifies that the parser output is structurally compatible with
the saturation engine without an explicit conversion layer -- the
`Term` values it produces (TAG_CTR / TAG_FVR with the right symbol IDs)
flow straight into the engine.

`tests/test_wald.c`: 226 sub-checks (was 214).

### Added: top-level Waldmeister .pr parser driver (stage 6.3f)

`wald_parse(src, spec) -> WaldErr` lands in `src/wald/_.c`.
Lexes the source, finds the first section keyword via
`wald_skip_to_section`, then dispatches each section to its
parser (NAME / MODE / SORTS / SIGNATURE / VARIABLES / ORDERING /
EQUATIONS / CONCLUSION).  Each parser returns the next section's
enum so the driver just chains.

Sections are accepted in any order -- Waldmeister's grammar
specifies a fixed order, but the parser is permissive (matching
upstream behavior and easing test fixture writing).  Per-section
parse errors don't bail the driver: the section parser falls
through to `wald_skip_to_section` so we still consume the rest
of the file and produce a partial-but-coherent spec.

`WaldErr` enum: `WALD_OK`, `WALD_ERR_NULL`, `WALD_ERR_NO_SECTION`.

Tests in `tests/test_wald.c` (214 sub-checks) include:
- NULL args -> `WALD_ERR_NULL`
- empty source -> `WALD_ERR_NO_SECTION`
- "foo bar baz" with no recognized keyword -> `WALD_ERR_NO_SECTION`
- the full group-axiom `.pr` file from
  `waldmeister/documents/example.pr` parses end-to-end:
  - `name == "group"`, `mode_proof == 1`
  - 4 symbols (e/i/f/a) with arities 0/1/2/0 and the
    monotonic CTR labels
  - precedence ranks i=3, f=2, e=1, a=0 from the LPO section
  - 3 variables (x/y/z) with sequential FVR ids
  - 3 axioms in `eqn_lhs/rhs[]`
  - goal_lhs is `f(...)` (label of f), goal_rhs is `e`

This unblocks 6.3g (named end-to-end test) and 6.4 (run
saturation on the parsed spec, emit a PCL trace).

### Added: EQUATIONS + CONCLUSION parsers (stage 6.3e)

`wald_parse_equations` and `wald_parse_conclusion` land in
`src/wald/_.c`.  Both share a small `wald_parse_equation_pair`
helper that reads `term "=" term`, returning 1 on success.

EQUATIONS appends each pair to `spec->eqn_lhs/rhs[]` (capped at
`WALD_MAX_EQNS`).  CONCLUSION writes only the FIRST pair into
`goal_lhs/goal_rhs`; subsequent pairs in the same section are
parsed (so the section terminates correctly) but discarded.
This matches the proof-mode constraint of one conjecture per
spec; multi-conclusion is a 8.x revisit.

Both share the same recovery pattern as 6.3c2..c5: peek for
end-of-section keyword, fall through to `wald_skip_to_section`
on parse error so downstream parsers still get the next
section's keyword.

Tests in `tests/test_wald.c` (183 sub-checks) cover:
- three-axiom EQUATIONS (`f(x, e) = x  f(x, i(x)) = e
  f(f(x, y), z) = f(x, f(y, z))  CONCLUSION ...`) with cross-
  check on the first pair's CTR/FVR structure
- empty EQUATIONS section (immediate `CONCLUSION` keyword)
- single CONCLUSION storing the goal
- reject-multiple: `a = e  i(a) = a` keeps only `a = e`

### Added: Waldmeister .pr term parser (stage 6.3d)

`wald_parse_term(spec, lex) -> Term` lands in `src/wald/_.c`.
Grammar:

  term ::= ident                            -- var (FVR) or
                                               zero-arity sym
        |  ident "(" term ("," term)* ")"   -- application

Variable-vs-symbol dispatch: an ident is a variable iff it
appears in `spec->vars[]` (returns `term_new_fvr(var_id)`);
otherwise it's a signature symbol that becomes a TAG_CTR with
the registered label.  Application arity must match the
signature -- mismatched calls return 0.

Recursive-descent; arg list capped at `REWRITE_MAX_ARITY`.
Returns 0 on any parse error: unknown ident, missing close
paren, arity mismatch, or argument list on a 0-arity constant.

Tests in `tests/test_wald.c` (166 sub-checks) cover variable
lookup, zero-arity constant, two-arg application, nested
`f(i(x), e)`, unknown-ident, arity mismatch, and
constant-with-args paths.

### Added: ORDERING section parser (stage 6.3c5)

`wald_parse_ordering(spec, lex)` lands in `src/wald/_.c` with a
new `prec_rank` field on `WaldSym`.  Grammar:

  "KBO" weight_list precedence
  "LPO" precedence

Where `weight_list = name = number, name = number, ...` and
`precedence = f1 > f2 > ... > fN` (left = greatest).

The parser reads everything as a token stream, tracking the most
recently seen ident; on `>` the previous ident becomes the next
chain entry.  KBO weight lists are consumed and discarded (the
saturation engine's `KboConfig` stays caller-supplied; the .pr
file only contributes the precedence ordering).  After the
chain is gathered, ranks are assigned so chain[0] gets
`prec_rank = N - 1` (greatest) and chain[N-1] gets
`prec_rank = 0` (smallest).

Stage 6.3c (a/b/c1/c2/c3/c4/c5) now complete.  Next: 6.3d term
parser, then 6.3e/f/g.

Tests in `tests/test_wald.c` (147 sub-checks) cover:
- LPO chain `i > f > e > a` -> ranks i=3, f=2, e=1, a=0
- KBO weights `i=0, f=1, e=1, a=1` followed by the same chain
  -> identical ranks (weights ignored)
- empty ORDERING (immediate next-section keyword) -> ranks
  stay at calloc'd 0
- lone ident with no `>` -> not added to the chain, ranks 0

### Added: VARIABLES section parser (stage 6.3c4)

`wald_parse_variables(spec, lex)` lands in `src/wald/_.c`.
Grammar:

  { ident { "," ident } ":" sort_ident }

Each ident gets registered into `spec->vars[]` with a sequential
FVR id (`var_id = spec->n_vars` at registration time).  Sort
names are consumed and discarded under the homogeneous-signature
assumption.  Multiple `name : sort` decl groups in one section
accumulate ids monotonically.

Section ends at the next section keyword via the same peek +
`wald_skip_to_section` recovery pattern as 6.3c2/c3.  EOF
mid-list returns `WSEC_NONE`; whatever names were registered
stay registered.

Tests in `tests/test_wald.c` (131 sub-checks) cover:
- `x,y,z : ANY EQUATIONS foo` -> 3 vars (ids 0/1/2), returns
  EQUATIONS, lexer past `EQUATIONS` at `foo`.
- empty VARIABLES (immediate `EQUATIONS`) -> 0 vars.
- multi-decl `x,y : ANY  z,w : ANY1  EQUATIONS` -> 4 vars
  (ids 0/1/2/3) flowing across both decl groups.
- truncated `x,y` -> WSEC_NONE, 2 vars registered.

### Added: SIGNATURE section parser (stage 6.3c3)

`wald_parse_signature(spec, lex)` lands in `src/wald/_.c`.  Per
entry parses `name : arg_sort1 ... argN -> result_sort` and
registers a `WaldSym { name, label = next_label++, arity = N }`
in `spec->symbols[]`.  The result sort is consumed and discarded
under the homogeneous-signature assumption for stages 5-7.

Section ends at the next section keyword via the same peek +
`wald_skip_to_section` recovery pattern as 6.3c2.  Capacity
overflow (`n_symbols >= WALD_MAX_SYMBOLS`) and parse errors fall
through to `wald_skip_to_section` so downstream parsers still
see the next section keyword.

Tests in `tests/test_wald.c` (109 sub-checks) cover:
- single zero-arity entry: `e: -> ANY ORDERING ...` -> e at
  label 1, arity 0; lexer past `ORDERING`.
- three entries with monotonic labels: e=1, i=2, f=3; arities
  0, 1, 2.
- empty SIGNATURE (immediate `ORDERING` keyword): no entries.
- truncated mid-entry (`f: ANY ANY ->` without result sort):
  returns WSEC_NONE, half-parsed entry not committed.

### Added: NAME / MODE / SORTS section parsers (stage 6.3c2)

`src/wald/_.c` lands three parsers with the same shape:

- `wald_parse_name`  -- one ident -> `spec->name`
- `wald_parse_mode`  -- one ident; `"COMPLETION"` -> `mode_proof
  = 0`, anything else (or empty) -> `mode_proof = 1`
- `wald_parse_sorts` -- ident list consumed and discarded
  (homogeneous-signature assumption for stages 5-7)

Each peeks for an immediate section keyword (empty section ->
return next section's enum without touching `spec`), otherwise
consumes its content + falls through to `wald_skip_to_section`.
Internal helper `wald_consume_if_section` factors the empty
check.

Tests in `tests/test_wald.c` (88 sub-checks) cover the populated
path (`name == "group"`, `mode_proof = 1` for PROOF / 0 for
COMPLETION, sorts consumed leaving the lexer at the next
section), the empty-section path (immediate next keyword leaves
defaults intact), and the EOF path (returns `WSEC_NONE`).

### Added: section-detect infrastructure for the .pr parser (stage 6.3c1)

`src/thvm.h`:

- `WaldSection` enum: `WSEC_NONE` plus `NAME / MODE / SORTS /
  SIGNATURE / VARIABLES / ORDERING / EQUATIONS / CONCLUSION`.
- `WaldLex` gains a 1-token peek (`have_peek` flag +
  `peeked_kind` + `peeked_text` + `peeked_len`).
- `wald_section_from_ident(name) -> WaldSection`,
  `wald_lex_peek(lex)`, `wald_skip_to_section(lex)`.

`src/wald/_.c`:

- `wald_lex_init` clears the peek slot.
- `wald_lex_next` consumes the peek if armed (copies
  peeked_text into tok_text), otherwise scans normally.
- `wald_lex_peek` populates the peek using the regular scan
  path; idempotent until the next consume.
- `wald_section_from_ident` is a flat strcmp dispatch,
  case-sensitive (".pr" files use uppercase keywords).
- `wald_skip_to_section` eats tokens until it sees a known
  section keyword; returns the matching enum, or WSEC_NONE on
  EOF.

This is the shared dependency for 6.3c2..c5: each section parser
falls back to `wald_skip_to_section` on unrecognized content and
uses `wald_lex_peek` to detect end-of-section without consuming
the next section's keyword.

Tests in `tests/test_wald.c` (76 sub-checks) cover all eight
section keywords, the unknown / case-sensitive / empty-string
paths for `wald_section_from_ident`, peek-then-next consistency,
peek-at-EOF idempotence, `wald_skip_to_section` landing past the
keyword, and the EOF + empty-source paths.

### Added: Waldmeister .pr lexer (stage 6.3b)

`src/wald/_.c` gains the lexer half of the parser: `WaldLex`
cursor over a NUL-terminated source buffer plus
`wald_lex_next(lex) -> WaldTokKind`.

Token kinds: `WT_END`, `WT_IDENT` (ident text in
`lex.tok_text[]`, truncated to `WALD_NAME_LEN - 1`), `WT_COLON`,
`WT_ARROW` (`->`), `WT_EQ`, `WT_LPAREN`, `WT_RPAREN`,
`WT_COMMA`, `WT_GT`, `WT_ERR`.  Skips whitespace and
`%`-to-end-of-line comments; section keywords (NAME, MODE,
SORTS, SIGNATURE, ORDERING, VARIABLES, EQUATIONS, CONCLUSION,
LPO, KBO) come back as plain `WT_IDENT` -- the section drivers
(6.3c) compare `tok_text`.

Tests in `tests/test_wald.c` (49 sub-checks) cover empty input,
whitespace-only input, `%` comment skipping, ident chars
including digits + underscore, `->` vs bare `-` (errors), the
full punctuation set, an `f(x, e) = x` token stream, long-ident
truncation (`tok_len == NAME_LEN - 1`, NUL-terminated), and the
unknown-char error path.

### Added: WaldSpec data model for the .pr parser (stage 6.3a)

`src/wald/_.c` lands the Waldmeister-spec data container plus
`wald_init` (heap-allocated, defaults to `mode_proof = 1` and
`next_label = 1`) and `wald_free` (NULL-safe).  Public types in
`src/thvm.h`: `WaldSym`, `WaldVar`, `WaldSpec`, plus caps
`WALD_MAX_SYMBOLS = 64`, `WALD_MAX_VARS = 32`, `WALD_MAX_EQNS =
64`, `WALD_NAME_LEN = 32`.  The struct holds the parsed
signature (each symbol gets a monotonic CTR label starting at 1
to skip the anonymous-tuple label), variable table (sequential
FVR ids), parallel `eqn_lhs/rhs[]` axiom arrays, and a single
`(goal_lhs, goal_rhs)` for proof-mode CONCLUSION.

Lexer / section drivers / term parser / equations / driver land
in 6.3b..f; end-to-end test against the group example follows in
6.3g.

### Added: PCL-shaped trace serializer (stage 6.2)

`thvm_atp_trace_serialize(s, buf, cap)` walks `s->trace[]` and
emits Waldmeister-PCL-style text into `buf`.  Each entry becomes
one line:

  <idx> (<reason> [from <p_a>[, <p_b>]]): <lhs> = <rhs>

Internal `atp_pretty_term` recursively prints CTR / FVR / NUM /
ERA terms with a "?T<tag>" fallback for the rest.  Truncates
silently on buffer overflow; the returned byte count gives the
caller a way to detect truncation against `cap - 1`.

Mirrors the role of Waldmeister's `pcl.c`
(*Proof Construction Language*) output -- a flat per-step record
that downstream tools can re-render into LaTeX / ASCII / Prolog
proofs.

Tests in `tests/test_atp.c` (8342 sub-checks) cover:
- empty trace yields zero bytes + null-terminated buf
- single axiom prints `0 (axiom): C... = C...`
- f(x, e) renders as `C3(x_0, C1)`; rhs as `x_0`
- post-step trace contains `1 (orient from 0):` and
  `... (cp from 1, 1): ...` lines
- 16-byte buffer truncation stays null-terminated

### Added: trace-walk verification on the headline demo (stage 6.1d)

`atp/headline-trace-shape-and-walk-to-axiom` in
`tests/test_atp.c` runs the same group-axiom proof as the stage
5.5 headline test and asserts the trace produced is sane:

- exactly 3 `TRACE_AXIOM` entries (the three axioms we pushed)
- at least 1 `TRACE_ORIENT` entry (the rule(s) added to R)
- the latest `TRACE_ORIENT`'s `parent_a` chain walks back
  through orient/CP entries to a `TRACE_AXIOM`, capped at 100
  hops to defend against pointer corruption

The walk-to-axiom invariant is the proof of correctness for the
trace plumbing: every rule in R can be tracked back to the
axioms that produced it, end-to-end, with no broken links.

Stage 6.1 (a/b/c/d) complete.  Next: 6.2 PCL-shaped serializer.

### Added: TRACE_CP entries from generate_cps (stage 6.1c)

`thvm_atp_generate_cps` rewritten to iterate `(i, j)` pairs
explicitly so each emitted CP knows the trace indices of the two
source rules.  Survivors push onto the queue with
`TRACE_CP(parent_a = r_trace[i], parent_b = r_trace[j])` -- a
new helper `atp_push_cps_traced` does the trace+queue push.

`AtpState` gains `u32 r_trace[ATP_MAX_RULES]` tracking each rule's
TRACE_ORIENT entry index.  Initialized to `ATP_TRACE_NONE` so
test code that pre-populates `s->lhs/rhs` directly produces
TRACE_CP entries with NONE parents (clearly marking them as
"source rule unknown" rather than mis-aliasing trace index 0).

`thvm_atp_step` now stashes each newly-added rule's TRACE_ORIENT
index in `r_trace[added.first + k]` so subsequent generate_cps
calls have the provenance.  `thvm_atp_interreduce` shifts
`r_trace[]` in lockstep with the `lhs/rhs` arrays when dropping
subsumed older rules.

Tests in `tests/test_atp.c` (8323 sub-checks) verify that after
adding one axiom and running one step, `s->trace[2]` is a
TRACE_CP entry whose `parent_a == parent_b == 1` (the orient
entry index of the new rule, which self-overlaps to produce the
trivial top-position CP).

### Added: trace wired into add_equation + atp_step orient (stage 6.1b)

`thvm_atp_add_equation` now pushes a TRACE_AXIOM entry (parents =
ATP_TRACE_NONE) and stashes its index in
`AtpState.cp_trace[i]` alongside the lhs/rhs slot.

`thvm_atp_select_cp` shifts the new `cp_trace[]` array in lockstep
with the lhs/rhs queue and writes the popped CP's trace index
into a transient `s->last_popped_trace` field for the saturation
step to consume.

`thvm_atp_step` reads `last_popped_trace` after `select_cp` and,
following a successful `orient_and_add`, pushes one TRACE_ORIENT
entry per added rule (the unfailing 2-way fallback gets two
entries, both linking back to the same source CP).

Tests in `tests/test_atp.c` verify:
- `add_equation` populates the trace with a TRACE_AXIOM whose
  parents are NONE; `cp_trace[0]` points back at it.
- After `thvm_atp_step` orients an axiom, the new trace entry is
  TRACE_ORIENT with `parent_a` pointing at the original axiom.

CP-generation traces (TRACE_CP) still pending in 6.1c.

### Added: trace storage + push helper for AtpState (stage 6.1a)

`src/thvm.h` gains `TRACE_AXIOM/ORIENT/CP` reason labels,
`ATP_TRACE_NONE = 0xFFFFFFFFu` parent-index sentinel,
`ATP_MAX_TRACE = 4096` cap, and a new `Term trace[]` + `u32
n_trace` pair on `AtpState`.

`src/atp/_.c` gains the internal `atp_trace_push(s, reason, p_a,
p_b, lhs, rhs)` helper which packs a trace entry as
`TAG_CTR(label = reason, children = [NUM(p_a), NUM(p_b), lhs,
rhs])` -- IC-native shape so 6.2's PCL serializer can walk the
trace as plain heap terms.  Returns the index of the new entry,
or `ATP_TRACE_NONE` on overflow.

The helper is not yet wired into `add_equation` /
`orient_and_add` / `generate_cps` -- those land in 6.1b/c.
Storage is zero-init via `thvm_atp_init`'s calloc.

Tests in `tests/test_atp.c` cover entry decoding (AXIOM with both
parents NONE), parent threading (ORIENT pointing back at an
earlier AXIOM), and the overflow path (push #4097 yields
ATP_TRACE_NONE).

### Added: stage-5 headline saturation demo green (stage 5.5)

`atp/headline-prove-f-a-ia-equals-e-from-group-axioms` in
`tests/test_atp.c`: under the standard group-axiom KBO config
(weights `i=0, f=1, e=1, a=1`; precedence `i > f > e > a`;
`w0 = 1`), `thvm_atp_run` proves the conjecture
`f(a, i(a)) == e` from the three axioms

  right-id:  f(x, e)        = x
  right-inv: f(x, i(x))     = e
  assoc:     f(f(x, y), z)  = f(x, f(y, z))

in <= 20 saturation steps.  Stage 5 of
`docs/plans/waldmeister_ic_atp.md` complete: the IC-native ATP
saturation engine -- INC-priority CP selection (5.3),
KBO orientation with unfailing fallback (5.2b), interreduction
(5.2c), CP generation (5.2d), goal check (5.2e), and recursive-
descent rewriting (5.4) -- proves a real group-theory lemma
end-to-end.

### Changed: thvm_rewrite_step now recursive-descent (stage 5.4)

`thvm_rewrite_step` upgrades from top-only to outermost-leftmost
descent: it tries the top first; on no top-match, it walks the
TAG_CTR children left-to-right, recursing into the first child
that yields a rewrite, and returns the rebuilt term.  One step
still fires exactly one redex; `thvm_rewrite_normalize`'s
fixpoint loop drives multi-level reductions.

Effect on the saturation pipeline (stage 5):

  thvm_atp_interreduce   (5.2c) -- now drops rules whose LHS has
                                   a reducible sub-position, not
                                   just rules whose top reduces
  thvm_atp_goal_check    (5.2e) -- compound goals normalize fully
  thvm_atp_step          (5.2f) -- the per-step normalize covers
                                   all positions in the picked CP

No external API changes; the saturation pipeline inherits the
wider coverage automatically.  All existing top-only test cases
still pass (top is tried first, so top-position rewrites don't
get pre-empted by deeper ones).

Three new cases in `tests/test_rewrite.c` exercise sub-position
firing (`i(f(a, e))` -> `i(a)` under `f(x, e) -> x`),
multi-level (`i(i(f(a, e)))` -> `i(i(a))`), and top-tried-before-
children precedence.

### Changed: thvm_atp_select_cp now priority-aware via INC + collapse_ordered (stage 5.3)

Replaces the FIFO pop with the SupGen-style priority encoding from
the design memo: each queued CP becomes
`INC^k(CTR_label=idx [lhs, rhs])` where
`k = symbol_count(lhs) + symbol_count(rhs)` (the `--add`
heuristic from Waldmeister's `ClasHeuristics.c`
"classification heuristics").  All wrapped CPs are folded into a
SUP tree and run through `thvm_collapse_ordered`; the cheapest
leaf comes out first, its CTR label decodes back to the original
queue index, and we pop that index.

Singleton case skips the SUP/INC plumbing.  The pre-existing
saturation pipeline (5.2a..5.2f) consumes the upgraded selector
unchanged -- the `atp/run-one-step-prove` headline still passes.

The existing FIFO test in `tests/test_atp.c` is upgraded to
`select-cp-priority-order`: under k_1=4, k_2=k_3=2 the pop order
is l2 (k=2, dfs=1) -> l3 (k=2, dfs=2) -> l1 (k=4) rather than
the original l1 -> l2 -> l3 FIFO sequence.

### Fixed: TMemoryPlanGantt y-axis -- "BarHeight"->"Log" actually works

The `"BarHeight"->"Log"` option was documented in
`TMemoryPlanGantt::usage` but unwired: `linearScanPack` hardcoded
raw nbytes for slot height, so sub-1-KiB bufs in linear-train
all stacked at y=0 and a single preserved buffer dominated the
whole page.  Threaded the option through (default "Log") +
made y-axis tick labels mode-aware (Log mode shows
`(2^y - 1)/1024` KiB).  Each buf now gets a distinct y-stripe,
proportional to log2 of its size.

### Added: thvm_atp_step + thvm_atp_run -- saturation loop driver (stage 5.2f)

`src/atp/_.c` glues 5.2a..5.2e into the full step.  Order from
`docs/plans/saturation_loop.md` sec.2:

  goal_check -> step_cap -> select_cp -> normalize ->
  trivialize -> orient_and_add -> interreduce (with post-
  interreduce range adjustment so generate_cps targets the
  correct, possibly-shifted slots) -> generate_cps -> goal_check.

`thvm_atp_step` returns one of ATP_PROVED / ATP_TIMEOUT /
ATP_QUEUE_EMPTY / ATP_RUNNING.  `thvm_atp_run` is the trivial
loop wrapper.

The interreduce-shifts-new-rule-indices coordination matters: when
`thvm_atp_interreduce` drops `dropped` older rules, the freshly-
added rules' indices each move down by `dropped`, so
`thvm_atp_generate_cps` is called with `post.first =
added.first - dropped` to re-derive the right (new x R) and
(old x new) sweeps.

Tests cover empty queue (returns QUEUE_EMPTY), trivial `e == e`
goal (PROVED at the top-of-step check, no work done),
`step_cap == 0` with non-trivial goal (TIMEOUT), the headline
one-step prove (`f(a, e) == a` from axiom `f(x, e) = x` runs to
ATP_PROVED via `thvm_atp_run`), and completion-mode saturation
that exhausts the queue and returns QUEUE_EMPTY.

### Added: thvm_atp_goal_check (stage 5.2e)

`thvm_atp_goal_check(s)` normalizes both sides of `s->goal_{lhs,
rhs}` under R via `thvm_rewrite_normalize` (NORM_CAP = 64) and
returns ATP_PROVED on a `kbo_eq` hit, ATP_RUNNING otherwise.
Skips cleanly (returns ATP_RUNNING) when `goal_lhs == 0` --
the completion-mode case where there's no conjecture to prove.

Top-only rewriting today; 5.4's recursive descent will widen
coverage to compound goal terms automatically.

Tests cover the no-goal pass-through, the trivial `e == e` case
that proves under empty R, the close-under-one-rule case
(`f(a, e) == a` under `f(x, e) -> x`), and the doesn't-close
case (`a == e`, no applicable rule).

### Added: thvm_atp_interreduce (stage 5.2c)

`thvm_atp_interreduce(s, added)` walks the older rules in `R`
(indices `[0, added.first)`), normalizes each LHS under the
freshly-added rule(s), and drops any rule whose LHS simplifies.
The dropped rule's `(reduced, old_rhs)` equation is pushed back
onto the CP queue for the saturation loop to re-orient under the
smaller `R`.

Top-only rewriting today via the existing
`thvm_rewrite_normalize`; stage 5.4's recursive descent widens
coverage to sub-positions without changing this function.

The new rules are copied out by Term value before the loop so the
array can be compacted without invalidating the dispatch.  Mirrors
Waldmeister's `Interreduktion.c` ("interreduction") cleanup phase.

Tests cover empty-added, drop-on-specialization (an `f(a, e) ->
f(a, a)` rule disappears under a fresh `f(x, e) -> x`),
keep-on-irreducible (a rule with a different top symbol survives),
and the underflow guard for the first-ever rule add (`added.first
== 0`).

### Added: thvm_atp_generate_cps + thvm_critical_pairs_range (stage 5.2d)

`src/cp/_.c` gains `thvm_critical_pairs_range(lhs, rhs, n,
start_i, end_i, start_j, end_j, out, cap)` -- generates CPs
restricted to a sub-rectangle of the rule index space.  The
existing `thvm_critical_pairs` becomes a thin wrapper passing the
full extents.

`src/atp/_.c` gains `thvm_atp_generate_cps(s, added)` which uses
the range version twice -- (new x all_R) then (old x new) --
to compute exactly the freshly-required CPs after a rule add,
skipping the (old x old) work that's already in the queue.
Temp buffer `ATP_CP_BATCH = 1024` CPs; survivors pushed onto the
queue, overflow dropped silently (matches Waldmeister's
*Kritische-Paare-Verwaltung* "critical-pair management" in
`KPVerwaltung.c`).

Tests cover empty-added no-op, single-rule self-overlap producing
at least one CP, an old-times-new sweep over (assoc + left-id),
and equivalence between the full and range versions.

### Added: thvm_atp_orient_and_add (stage 5.2b)

KBO-orient an equation and push the resulting rule(s) onto `R`.
Returns `AtpAddedRange { first, count }` so the next saturation
phase (5.2d generate-CPs) can target only the new rules.

- `KBO_GT`: push `lhs -> rhs`                                 (count = 1)
- `KBO_LT`: push the swap `rhs -> lhs`                         (count = 1)
- `KBO_UN`: unfailing fallback -- push both orientations,
  atomic on capacity (skip both if there's room for only one)   (count = 2)
- `KBO_EQ` or `R` full: no-op                                  (count = 0)

The unfailing variant of Knuth-Bendix completion (Bachmair-
Dershowitz-Plaisted) keeps unorientable equations as 2-way rules
so rewriting can try either direction.  Mirrors Waldmeister's
behavior; see `waldmeister/sources/INF/Hauptkomponenten.c`
(*Hauptkomponenten* = "main components").

### Added: thvm_atp_select_cp FIFO pop (stage 5.2a)

`thvm_atp_select_cp(s, &lhs_out, &rhs_out)` lands in
`src/atp/_.c` -- pops the front CP, shifts the tail down to
keep the array dense.  Returns 1 on success / 0 on empty.
Stage 5.3 will replace the FIFO with priority-collapse over
INC-wrapped CPs (the `--add` heuristic from
`waldmeister/sources/CLAS/ClasHeuristics.c`).  Tests cover
empty queue, FIFO order across three pushes, and tail
densification after a pop.

### Added: AtpState struct + init/free helpers (stage 5.1)

`src/atp/_.c` lands the saturation-loop state container plus
`thvm_atp_init` (heap-allocates, stores cfg + step_cap),
`thvm_atp_free` (NULL-safe), `thvm_atp_add_equation` (push CP),
`thvm_atp_set_goal`.  Public types in `src/thvm.h`: AtpStatus
enum, ATP_MAX_RULES (256), ATP_MAX_CPS (4096), AtpState struct
(rules R, CP queue, goal, KboConfig, step counter).  Tests in
`tests/test_atp.c` cover init/free symmetry, NULL-free safety,
queue-full rejection, goal set/clear.  Step + run drivers are
5.2.

### Changed: f1d helper accepts REDUCE-as-tail-op (CPU)

`materialize_kernel_inlined` (CPU-only via the existing backend
gate) now accepts `root_op == UOP_REDUCE` when the source is a
fully-inlinable elementwise chain.  Tinygrad's "local reduction"
pattern: one kernel runs N-1 elementwise ops into a register and
the final REDUCE writes the output buffer.

Linear-train forward+loss with toggle ON: 16 -> 8 kernels.  Both
the Softmax-normalization REDUCE_SUM(EXP(x)) and the CE-loss
REDUCE_SUM(MUL(target, LOG(p))) chains now collapse into one
kernel each instead of one kernel per UOp.

### Added: saturation-loop design sketch (stage 5.0)

[docs/plans/saturation_loop.md](docs/plans/saturation_loop.md)
designs the AtpState struct, the 10-step saturation algorithm,
fairness mitigations (step_cap + round-robin escape), termination
conditions, and the mapping from existing C-side primitives
(`thvm_match`, `thvm_unify`, `thvm_critical_pairs`,
`thvm_rewrite_normalize`, `thvm_kbo`, `thvm_collapse_ordered`)
into the loop body.  The implementation lands in 5.1-5.4; demo
(prove `f(a, i(a)) = e` from group axioms) is 5.5.

### Verified: ATP arc baseline green (stage 0 sanity)

`make test` (48 C executables, 166 sub-checks) and `make wl-test`
(295 WL VerificationTests) both green at HEAD `f49f267`.  First
firing of cron `757c483c` driving
[docs/plans/waldmeister_ic_atp_tasks.md](docs/plans/waldmeister_ic_atp_tasks.md)
through stages 5-8+.

### Added: ICC type-flow primitives (TAG_BRI + TAG_ANN, real ICC rules)

`TAG_BRI = 23` (Bridge / Val: θx.body) and `TAG_ANN = 24`
(Annotation: {val : typ}) land with the actual ICC reduction rules
from `TinyHVM/resources/gists/icc_spec.md`, not the LAM-alias that
TinyHVM shipped with:

  APP (θx.body) arg = θx (APP body[x ← λ$k.x]  (ANN $k arg))
  ANN val (λx.body) = λx (ANN (APP val $k) body[x ← θ$k.x])
  ANN val (θx.body) = body[x ← val]                          (type erasure)

Plus DUP-BRI commutation (mirror of DUP-LAM) so bridges duplicate
correctly under SUP search.

Files: `src/term/new_bri.c`, `src/term/new_ann.c`,
`src/interact/{app_bri,ann_lam,ann_bri,dup_bri}.c`.  TAG_ANN
reduction is inline in `src/wnf/_.c` (mirrors TAG_OP2's strict-
on-typ-then-dispatch pattern).

Tests (`tests/test_icc.c`, 11 sub-checks):
- ANN-BRI type erasure on θx.x consumes the bridge, leaves the val
- ANN-BRI on a bridge whose body is its own bound y returns y[y ← val]
- APP-BRI on θx.x with NUM(7) fires and the head is again BRI
  (the inner structure changes; ICC is type-flow, not value-flow)
- ANN-LAM fires and the head wraps in a new λ
- DUP-BRI commutes !&7{F0,F1} = θx.x into two bridges
- ANN with a non-LAM/BRI typ stays stuck

These are the ICC primitives the IC-native ATP plan can fall back
on for closed-form encodings of equations + dependent-type proofs.
FVR-based open-form remains the active path for stages 2-4 of the
plan; BRI/ANN are now ready when stages 5-7 motivate them.

### Added: stage 4 -- unification + critical-pair enumeration

`src/unify/_.c` lands the Robinson MGU on TAG_CTR + TAG_FVR with
the standard occurs check.  Result lives in the same RewriteSubst
struct used by stage 3's matcher; `unify_walk` follows FVR -> FVR
chains, and `thvm_unify_apply` realizes a chained substitution
into a fully-instantiated term.  `thvm_rename_vars(t, offset)`
shifts every FVR id by `offset` so two rules can be unified
without variable-name collisions.

`src/cp/_.c` enumerates critical pairs.  Walks every non-variable
position of `rule_i.lhs`, tries unifying with `rule_j.lhs`
(renamed apart by `REWRITE_MAX_VAR/2`), and emits
`(σ(l_i[p ← r_j]), σ(r_i))` on success.

Demo (`tests/test_cp.c`): rule set `{ f(e, x) = x ; f(f(x, y), z)
= f(x, f(y, z)) }` produces the expected CP `(f(y, z), f(e, f(y,
z)))` from the [0]-overlap of left-id into assoc.

C-side only for now; SUP-encoded CP enumeration via a `TAG_PRI`
unify primitive (stage 4.5) is optional and deferred.

### Added: TMatStatsLabel for per-realize THVM_MAT_STATS attribution

`TMatStatsLabel["fwd_conv1"]` tags the next `thvm_realize` call's
`THVM_MAT_STATS=<path>` log line with the given string; the buffer
clears after one realize.  Bridges:
`thvm_wl_mat_stats_label(UTF8String)` in `wl/THVMLink/CSource/thvmlink.c`,
backed by a 64-byte `MAT_STATS_LABEL` global in `materialize_memo.c`.

Lets probes attribute kernel counts to specific layers / grad
chains.  Sample LeNet breakdown: forward+loss=231 kernels;
grad_b1..b3=20 each; grad_w4=40; grad_b4=36 -- forward Conv2D-
lowered chain dominates and is the next fusion target.

### Changed: lenet bench + verify use TGradMany; materialize descends into TAG_CTR

`lenetStep` (baseline.wls) and `stepGrads` (verify.wls) now build a
single multi-target `UOP_GRAD` via `TGradMany[loss, weights]`.
`materialize_expr` gained a `TAG_CTR` case that recursively
materializes each child within the same realize, so all n backward
kernels emit in ONE materialize pass with shared memo.

Bench result is NEGATIVE for the kernel-count metric (427 -> 426
on lenet) and 0% for peak.  Cause: each per-target chain rule
allocates fresh cotangent UOp cells with new heap locs, so the
memo can only dedup the forward leaf references.  Detail +
follow-up options in `docs/bench-results.md` "k0e" section.

### Added: equational rewriter -- stage 3 (one-shot, top-position only)

`src/rewrite/_.c` exports a small C-side equational rewriter for
TAG_CTR + TAG_FVR terms:

- `thvm_match(pattern, term, subst)` -- one-way matching with
  linearity check (a variable seen multiple times must bind to
  the same sub-term, verified via `kbo_eq`).
- `thvm_subst_apply(t, subst)` -- substitution-with-rebuild: TAG_FVR
  becomes its bound sub-term; TAG_CTR is rebuilt with substituted
  children; everything else passes through.
- `thvm_rewrite_step(t, lhs, rhs, n_rules)` -- try each rule in
  order at the *top* position; first match wins, RHS returned with
  substitution applied.
- `thvm_rewrite_normalize(t, lhs, rhs, n_rules, step_cap)` -- iterate
  rewrite_step to a fixpoint or until step_cap exhausts.

Recursive descent into sub-terms is not yet wired -- that's part of
the saturation loop in stage 5.

The headline demo from `docs/plans/waldmeister_ic_atp.md` sec.5
runs in `tests/test_rewrite.c`: under the full group axioms

  f(x, e)        = x
  f(x, i(x))     = e
  f(f(x,y), z)   = f(x, f(y, z))

`f(a, e)` normalizes to `a` (one rewrite_step fires; the second
step is a fixpoint).  Plus 8 supporting cases for matching,
non-linear consistency, substitution, no-applicable-rule, and the
inverse rule firing.

### Added: TAG_FVR + thvm_kbo -- stage 2 (term encoding + KBO ordering)

`TAG_FVR = 22` is an atomic first-order variable: `EXT = var_id`,
no heap cells.  Distinct from `TAG_VAR` (the IC's bound variable
tied to a binder).  Used by the IC-as-ATP layer to encode the
universally / existentially quantified variables of equational
logic.

`thvm_kbo(s, t, cfg)` (`src/kbo/_.c`) implements the Knuth-Bendix
ordering on TAG_CTR + TAG_FVR terms.  KboConfig holds per-symbol
weights, total precedence, and the scalar variable weight w0.
Returns KBO_EQ / KBO_GT / KBO_LT / KBO_UN.  Algorithm: Baader-
Nipkow (variable-domination check, weight comparison, top-symbol
precedence tiebreak, lexicographic on args).

The headline demo from `docs/plans/waldmeister_ic_atp.md` sec.5
runs in `tests/test_kbo.c`: under Waldmeister's default group-
axiom KBO (weights `i=0, f=1, e=1, a=1`; precedence `i > f > e > a`;
`w0 = 1`), `f(x, e) > x` orients correctly.

C-side only for now -- the IC-as-pure-program port (stage 2.4) is
optional and deferred.

### Added: TGradMany WL bridge

`TGradMany[y, {x_1, ..., x_n}]` in `wl/THVMLink/Kernel/Tensor.wl`
builds a single `UOP_GRAD` and realizes once; the resulting
`TAG_CTR` of n cotangents is unpacked into a List of TTerm
wrappers via `thvm_wl_term_ctr_at`.  3 new tests in `grad.wlt`
assert equality with the per-target `TGrad` results.

### Added: multi-target chain rule for UOP_GRAD

`interact_grad` now handles `n>1` by lowering to a `TAG_CTR` of `n`
unary `uop_grad(y, gy, x_i)` terms.  Each unary grad walks the
chain rule independently; the forward DAG (y and its descendants)
lives at shared heap locs so materialize's per-realize memo dedups
every kernel emitted from those forward UOps across all `n`
targets.  `n=1` keeps the scalar return for backward compat.

### Changed: UOP_GRAD heap layout is now multi-target (k0b)

`uop_grad` heap is now `[y, gy, NUM(n), x_1, ..., x_n]` (was
`[y, gy, target]`).  New `uop_grad_multi(y, gy, targets, n)` is
the primary constructor; the legacy unary `uop_grad(y, gy, x)`
is a thin wrapper with `n=1`.  `interact_grad` bails on `n>1`
for now -- the multi-target chain rule lands in k0c.

The change cascades through every site that knows GRAD's heap
arity: `wnf/redex.c` `term_arity` reads `NUM(n)` to compute
`3+n`; `alo/realize.c` `alo_node_arity` and
`book/from_dynamic.c` `dyn_arity` take a `val` argument so they
can `book_read` / `heap_read` the count when cloning UOP_GRAD
templates (TOptim's recursive lambdas embed it).  Accessors
`uop_grad_n` / `uop_grad_target` provide read-side parity.

### Added: TAG_WHEN boolean filter -- closes the stage-1 e2e demo

`TAG_WHEN = 21` is the IC-side primitive for "collapse to the
matching one":

  WHEN(NUM(0), _)        -> ERA               (failed branch erases)
  WHEN(NUM(n != 0), b)   -> wnf(b)
  WHEN(ERA, _)           -> ERA
  WHEN(&L{c0,c1}, b)     -> &L{WHEN(c0, B0), WHEN(c1, B1)}, !&L{B0,B1}=b

The end-to-end demo from `docs/plans/waldmeister_ic_atp.md` now
runs in one IC reduction + one collapse:

```
cands = &L{NUM(2), NUM(3)}
t     = WHEN(EQL(cands, NUM(3)), cands_dup)
collapse(t) -> [NUM(3)]    -- only the matching candidate
```

Failed candidates collapse to ERA via WHEN-NUM-zero, and
`thvm_collapse` drops ERA branches.  This is stage 1.7 revised:
constructors+MAT deferred to stage 2 (term encoding) where they
are motivated by encoding equations.

Constructor: `term_new_when(cond, body)`.  Tests:
`tests/test_when.c` covers all rules + the e2e demo.

### Added: TAG_INC priority wrapper + thvm_collapse_ordered

`TAG_INC = 19` is a one-cell priority wrapper.  The reducer treats
it as a WNF atom (default fall-through; no interactions), so the
INC layer survives reduction and becomes visible to collapse.

`thvm_collapse_ordered(t, out, cap)` performs the same shallow
SUP-tree walk as `thvm_collapse`, but counts INC wrappers along
the path to each leaf and emits the leaves sorted by INC-depth
ascending (ties broken by DFS order).  Lower INC count = higher
priority = enumerated first.  Implementation collects (Term, pri,
idx) into a heap-allocated buffer, qsorts, writes Terms back.

This is the IC encoding of Waldmeister's `--mix` CP-selection
heuristic: wrap each candidate with INC^k where k is its weighted
cost, and `thvm_collapse_ordered` enumerates cheapest first.

Constructor: `term_new_inc(body)`.  Tests: `tests/test_inc.c`.

### Added: TAG_ANY wildcard

`TAG_ANY = 18` is an atomic wildcard.  Two interactions:

  EQL(ANY, x) -> NUM(1)        (matches anything, on either port)
  ! &L{x0,x1} = ANY  ->  x0 <- ANY, x1 <- ANY

Constructor: `term_new_any()`.  Used as the IC encoding of
existential / Skolem variables in the ATP plan.  Tests:
[tests/test_any.c](tests/test_any.c).

### Added: TAG_AND, TAG_OR with short-circuit + SUP commutation

`TAG_AND = 16` and `TAG_OR = 17` land as short-circuit boolean
nodes; both are strict on the left operand and lazy on the right:

  AND(NUM(0), _)        -> NUM(0)        (right stays unreduced)
  AND(NUM(n != 0), b)   -> wnf(b)
  AND(ERA, _)           -> ERA
  AND(&L{a0,a1}, b)     -> &L{AND(a0,B0), AND(a1,B1)}, !&L{B0,B1}=b

  OR(NUM(0), b)         -> wnf(b)
  OR(NUM(n != 0), _)    -> NUM(1)        (right stays unreduced)
  OR(ERA, _)            -> ERA
  OR(&L{a0,a1}, b)      -> &L{OR(a0,B0), OR(a1,B1)}, !&L{B0,B1}=b

The SUP commutation routes a superposed left operand through both
branches with the right operand DUPed, mirroring EQL-SUP.  This
enables the SupGen-style filter pattern `AND(EQL(cand, expected),
cand)`: the matching candidate survives, the rest become NUM(0).
Full ERA-propagating filter (collapse to *only* the matching
candidate) needs the MAT/constructor work in stage 1.7.

Constructors: `term_new_and(a, b)`, `term_new_or(a, b)` in
`src/term/`.  Tests: `tests/test_and_or.c`.

### Added: EQL-SUP commutation + DUP-NUM annihilation

The `EQL` reducer now commutes through `SUP` on either port:

  EQL(&L{a0,a1}, b)  ->  &L{EQL(a0, B0), EQL(a1, B1)}, !&L{B0,B1}=b
  EQL(a, &L{b0,b1})  ->  &L{EQL(A0, b0), EQL(A1, b1)}, !&L{A0,A1}=a

The DUPed b (resp. a) propagates correctly because `DUP-NUM`
annihilates atomically, copying the Term value into both
projections.  New file `src/interact/dup_num.c`.

End-to-end: `EQL(&L{NUM(2), NUM(3)}, NUM(3))` now reduces to
`&L{NUM(0), NUM(1)}`, and `thvm_collapse` enumerates `[NUM(0),
NUM(1)]` -- the SupGen-style search-as-superposition pattern is
working for the first time on thvm.

### Added: TAG_EQL (structural equality) -- minimal cut

`TAG_EQL = 15` lands as a strict equality node with heap layout
`[a, b]`.  The wnf reducer walks both ports to WNF and dispatches:

- `EQL(NUM(x), NUM(y))` -> `NUM(1)` if x==y else `NUM(0)`
- `EQL(ERA, _)` / `EQL(_, ERA)` -> `ERA` (failed branches collapse out)
- otherwise stuck

SUP commutation (the rule that pushes a SUP at either port up to
the head) lands separately in stage 1.3b alongside DUP-NUM.

Constructor: `term_new_eql(a, b)` ([src/term/new_eql.c](src/term/new_eql.c)).
Tests: [tests/test_eql.c](tests/test_eql.c).

### Added: glossary section on equational reasoning and the IC-as-ATP layer

[docs/glossary.md](docs/glossary.md) gains an *Equational reasoning
and the IC-as-ATP layer* table, explicitly distinguishing **HVM-SUP**
(the runtime data primitive `&L{a, b}`) from **ATP-superposition**
(the logical inference rule, refined paramodulation), plus
companion entries: collapse, label, substitution, **cosubstitution
and bisubstitution** (Wolfram's framing -- bisubstitution = paramodulation),
unification, matching, paramodulation, critical pair, Knuth-Bendix
completion, unfailing completion, reduction ordering, joinability,
saturation, subsumption, PCL.  The plan memo
[docs/plans/waldmeister_ic_atp.md](docs/plans/waldmeister_ic_atp.md)
gets a terminology warning at the top cross-referencing the new
section, and [docs/README.md](docs/README.md) lists the plan in its
plans-and-references index.

### Added: thvm_collapse -- shallow SUP-tree enumeration

`src/collapse/_.c` exposes `thvm_collapse(t, out, cap)` which walks
the head of `t` via WNF and recurses on TAG_SUP, dropping TAG_ERA
branches.  Caller-supplied buffer + cap; returns count.  This is
the "shallow" version: deeper enumeration through APP / OP2 / EQL /
... lands as those tags get SUP-commutation interactions.
Tests in `tests/test_collapse.c` cover single-leaf, single-SUP,
nested SUP, ERA-pruned branch, and cap-truncation.

### Added: docs/plans/waldmeister_ic_atp.md -- IC-native ATP design memo

Research-and-design memo summarizing Waldmeister's unfailing
Knuth-Bendix completion algorithm, surveying prior art on
interaction-net + ATP work (April 2026), and sketching how the same
proof procedure could be expressed as IC graph rewrites in thvm
using SupGen / NeoGen-style superposition over rule sets and
overlap spaces.  Includes a 7-stage build trajectory.

### Added: DUP-SUP cross-label commutation

`interact_dup_sup` now handles the commuting case
`!&L{x0,x1} = &R{a,b}` (L != R) by allocating a 6-cell block of
two new dup bodies (for `a` and `b`) plus four DP0/DP1 leaves, and
returning two fresh `&R`-labeled SUPs.  Previously the cross-label
case was stuck.  This unblocks any future tag whose interactions
need SUPs to flow through DUPs (EQL, AND/OR, MAT, INC, ...).
Tests in `tests/test_dup_sup.c` exercise head shape, inner
structure, and both-projection consistency.

### Changed: lazy GRAD + lazy materialize via shared term_resolve

`interact_grad` and `materialize_expr` no longer call `wnf` to
expose their inputs.  Both now route through a new shared
`term_resolve` (in `src/term/resolve.c`) that does the minimum
work needed to surface the outermost layer:

- TAG_VAR: take the SUB-bit cell (single-step deref); chase the
  chain if it cascades.
- TAG_ALO: force one realisation layer via `alo_force` (which is
  itself memoised, so repeated walks are cheap).
- everything else: return unchanged.

That's the entire resolver -- it does NOT fire materialize, kernel,
or grad reductions.  Anything `interact_grad` can't structurally
pattern-match (e.g., a free VAR that hasn't been bound yet) is
returned unchanged; `wnf`'s UOP_GRAD case got a fixed-point check
so the term sits as WHNF rather than re-fires.  `materialize_expr`
follows the same pattern, with a single `wnf` step retained for
the LAM/APP/REF case (where actual beta / unfolding is required
before any UOp shape is visible).

The SGD demo in `wl/THVMLink/Tests/sgd.wlt` was rewritten to drop
the per-iteration `TUOpMaterialize` wrapper from the loop body.
The recursive call now passes the symbolic `step(w)` UOp graph as
the new w; `TRealize` at the end fires one materialize over the
deeply-nested expression.  That's both cleaner and side-steps the
"shared TUOpMaterialize wrapper produces distinct fresh TENs per
fire so grad's leaf check breaks" issue we papered over with the
materialize cache: now every reference to `w` in the body is the
same UOp Term value, and the leaf check just works.

### Added: phase 3 -- SGD optimizer as a recursive lambda term

Tying phases 1 + 2 together to demonstrate the original use case
the user laid out: a lambda term that takes a "net with loss at
root" plus a parameter and adds GRAD nodes inside a recursive
training loop expressed as `TDef`/`TRef`.

Three runtime fixes were needed to make `materialize(step(w))`
compose multiple times without breaking grad's leaf check:

1. **Lazy GRAD chain rule.**  `interact_grad` used to recursively
   compute the entire chain rule expansion in one fire (eager).
   Now each fire does a single structural step on `y`'s outermost
   UOp, deferring sub-positions as fresh `UOP_GRAD` nodes that
   wnf re-enters on demand.  `wnf` is called ONCE on `y` and
   `target` to expose the outermost layer (so a `GRAD[APP(loss_fn,
   w), w]` body can beta-reduce before pattern-matching).  Existing
   numerics preserved (9/9 grad + 17/17 nn end-to-end tests still
   pass); test_grad.c structural assertions updated to expect the
   one-layer form.

2. **Materialize follows VAR substitutions.**  After APP-LAM beta,
   a UOP body's cells hold VARs pointing at the binder's
   substituted heap slot.  `materialize_expr` now wnfs each input
   first so VAR (and ALO and the active-path UOPs the wnf reducer
   knows about) resolve to a concrete TEN/UOP_KERNEL.

3. **Materialize result memoization.**  The same `MATERIALIZE`
   wrapper inside a single graph is often referenced from N
   slots (e.g. a recursive `step(w)` body uses `w` in both the
   loss and the weight update).  Each fire used to allocate a
   fresh kernel + TenDesc, so the resulting TAG_TEN ids differed
   per use; `interact_grad`'s `y == target` pointer-equality
   leaf check then failed and the gradient collapsed to zero.
   `thvm_materialize` now caches the realised result back into the
   wrapper's heap cell so subsequent fires return the SAME
   TAG_TEN id.

4. **ALO_force memoization.**  The companion fix for #3.  Each
   ALO fire used to re-realize from the book template, allocating
   fresh dyn cells.  In a recursive REF body that references the
   bound `w` multiple times, the multiple references then mapped
   to distinct fresh wrappers and #3 didn't help.  `alo_force`
   now writes the realised term back into the ALO cell and marks
   the second slot non-NUM as a "cached" sentinel.  Subsequent
   fires hit the cache.

WL example, in `wl/THVMLink/Tests/sgd.wlt`:

```
sgd_loop = TLam[w |->
  TLam[n |->
    TIfZero[n, w,
      TApp[
        TApp[TRef["sgd_loop"],
             TUOpMaterialize[
                w + (-lr) * grad(L2(w - target), w)]],
        TOp2["-", n, TNum[1]]
      ]
    ]
  ]
]
TDef["sgd_loop", sgd_loop]
TWnf @ TApp[TApp[TRef["sgd_loop"], w0], TNum[2]]
  -> {0.36, 0.72, 1.08}    (* w_2 = 0.8 w_1 + 0.2 target *)
```

4/4 SGD cases pass (one-step lambda + 0/1/2 recursive iters).
Compute scales steeply (kernels ~3-4x per iteration without DUP
sharing for tensors); training-scale runs need that next.

### Added: phase 2 -- MAT (numeric switch) + OP2 (binary ops on NUMs)

Two more term tags so a recursive REF body can hit a base case and
manipulate its iteration counter (precondition for the SGD-as-lambda
optimizer in phase 3):

- `TAG_OP2` (val = heap loc -> [x, y], ext = OP_*) -- strict on x
  then y; both must reduce to TAG_NUM for the op to fire.  Opcodes
  `OP_ADD` / `OP_SUB` / `OP_MUL` / `OP_EQ` / `OP_LT`.  Stuck if
  either operand stays non-NUM.
- `TAG_MAT` (val = heap loc -> [handler, fallback], ext = match)
  -- numeric-switch atom.  In wnf's `apply` phase, when an APP frame
  pops with a MAT head, the arg is forced via a recursive `wnf()`
  call: NUM matching `ext` reduces to `handler`, otherwise the
  result is `APP(fallback, arg)`.  Mirrors HVM4's APP-MAT-NUM.

`book/from_dynamic.c` and `alo/realize.c` learned the two new
fixed-arity-2 nodes so REF unfolding handles them.  `wnf/_.c` got
`case TAG_OP2` in enter and a `case TAG_MAT` branch inside the APP
apply switch.

WL surface in `wl/THVMLink/Kernel/Switch.wl`:
- `TNum[i]` / `TNum[i, dtype]` -- a TAG_NUM atom (defaults to i32).
- `TOp2["+"|"-"|"*"|"=="|"<", x, y]` -- a TAG_OP2 term.
- `TMatNum[matchVal, handler, fallback]` -- a TAG_MAT atom.
- `TIfZero[counter, then, else]` -- sugar that wraps the else branch
  in a discarding lambda so MAT's miss-path looks like a plain
  conditional.

Tests:
- `tests/test_mat_op2.c` (9 cases) -- OP2 arithmetic + MAT match /
  miss + an end-to-end **recursive countdown** built from
  REF + ALO + LAM/APP + MAT + OP2 (`@count 0 5 -> NUM(5)`).
- `wl/THVMLink/Tests/switch.wlt` (9 cases) -- the WL surface plus
  recursive countdown + sumto via `TDef`/`TRef`.  All pass.

All 331 C cases + 39 WL cases pass.

### Added: phase 1 of REF / ALO -- lazy named definitions

Two new term tags layered on top of the IC + UOP graph so users can
register named definitions and unfold them lazily during reduction
(precondition for the recursive SGD-as-lambda optimizer described in
PLAN.md):

- `TAG_REF` (val = name slot) -- a one-cell pointer into a fresh
  `DEFS[]` table holding the registered definition's *static
  template*.  Reducing a REF wraps the template in an empty-state
  ALO and re-enters; the body itself isn't expanded.
- `TAG_ALO` (val = dyn loc -> [book_term, NUM(state_id)]) -- the
  HVM4-style allocator.  Each fire walks one layer of the static
  template into a fresh dynamic heap region, threading an
  `AloState` chain that rebinds binders (LAM -> VAR) through the
  new dyn locs so multiple unfoldings of the same def don't alias
  each other's bound variables.

New runtime infrastructure:
- `BOOK_HEAP[]` (256K cells, parallel to `HEAP`) -- immutable
  per-def template cells.
- `DEFS[256]` -- root book term per registered name.
- `ALO_STATES[]` -- linked substitution chain for ALO descents.
- `book/{alloc,read,set,from_dynamic}.c` -- allocator + the
  recursive snapshot that lifts a dynamic term tree into the book
  heap (handles LAM / APP / VAR / fixed-arity UOP families; SUP /
  DUP / variable-arity movement ops are a follow-up).
- `alo/{state,realize,force}.c` -- the substitution chain plus
  `alo_realize` (one book-layer -> dyn) and `alo_force` (force a
  TAG_ALO term into its dyn shape).
- `term/{new_ref,new_alo}.c` -- term constructors.

`wnf/_.c` gained `case TAG_REF` / `case TAG_ALO` cases that fire
the unfolding; both bump `ITRS`.

WL surface in `wl/THVMLink/Kernel/Ref.wl`:
- `TDef[name, body]` -- snapshots `body` into the book heap and
  registers it under an integer slot (`name` may be a string -- it
  gets interned to a stable slot via `$defNames`).
- `TRef[name]` -- returns a TTerm wrapping a TAG_REF cell.
- `TDefName[name]` -- expose the slot mapping for tests.

Tests: `tests/test_ref.c` (5 cases) covers identity-via-REF + fresh
allocation per call + lazy self-reference; `wl/THVMLink/Tests/ref.wlt`
(4 cases) covers the WL surface end-to-end.  All 322 C cases + 30+
WL cases still pass.

Known scope: REF unfolds forever for self-referential defs without a
termination construct.  Phase 2 adds `MAT` (pattern match / numeric
switch) + `OP2` (SUB on NUMs for counter decrement) so a recursive
`train_step` lambda can hit a base case at iteration 0.

### Added: NN training-step numerics + per-render TimeConstrained budget

`nn.wlt` grew five training-flavoured cases on top of the layer
helpers:
- two-head square loss `(w.x + v.x)^2`, gradient sums across both
  paths to the same target;
- MSE through a dot product checked w.r.t. both `w` and `x`;
- one SGD step on `(w.x - t)^2` confirms the gradient direction
  reduces the loss;
- three-step gradient descent verifies loss is monotonically
  non-increasing;
- polynomial-regression-ish `(a x^2 + b x - t)^2` checks both
  partials.

`wl/Examples/run.wls` wraps each render in `TimeConstrained` (30 s
budget per heap-graph / IC-diagram render).  Dense tensor graphs
(NN-style compositions) sometimes blow the IC layout up by 100x and
hung the whole batch; now the over-budget render is skipped and
logged with `[skip] ... (over 30s)` so the rest of the examples
keep going.

### Added: NN.wl -- Wolfram NeuralNetworks layer -> TUOp graph converter

`wl/THVMLink/Kernel/NN.wl` lets users build the UOp graph by feeding
in built-in layers (`LinearLayer`, `ElementwiseLayer`, `NetChain`,
...) instead of inventing parallel layer constructors.  Tinygrad's
"Tensor + thin layer wrappers" model: a layer is a snapshot of
weights, the converter lifts them to TTensors and emits the same
TUOp* combinators users would write by hand.

Public surface (all in the THVMLink` context):
- `TFromNet[net, x]` / `TFromLayer[layer, x]` -- entry points,
  dispatch on `Head[layer]`.
- `TLayerWeights[layer]` / `TLayerToTensors[layer]` -- read a
  layer's NumericArrays / wrap them as TTensors.
- Tensor-method helpers: `TSum`, `TSquare`, `TDot`, `TMatVec`,
  `TL2Loss`, `TMSELoss`.

Currently supported layers:
- `LinearLayer[out, "Input" -> in]` -- forward via TMatVec
  (W @ x + b through EXPAND-broadcast + REDUCE_SUM).  Backward
  through W is a TODO until interact_grad gains an EXPAND rule.
- `ElementwiseLayer[#*# &]` -- maps to TSquare.  Adding more
  functions is a one-line entry in `$elementwiseDispatch`.
- `NetChain[{...}]` -- folds layers in declaration order.

Stubbed / out-of-scope:
- `ConvolutionLayer` -- raises a Message; needs movement-op
  support in materialize/interpret + the matching grad rules
  (step 14).

`wl/THVMLink/Tests/nn.wlt` covers the helpers (12 cases): forward
of LinearLayer / ElementwiseLayer / NetChain, plus end-to-end
gradient chains (TDot, TL2Loss, TMSELoss, polynomial, square of
dot product) -- all 12 pass.

### Fixed: shared-wire spiders for non-CONST UOP multi-reference

`wireFor` used to give every cell its own `w<loc>` wire name, so
when a non-CONST UOP fed N consumer slots only the principal cell
matched up; the other N-1 consumers dangled (visible in
`MUL[x, x]` where x is itself a UOP -- the second src wire had a
fresh name and stayed unconnected).

Fix: TAG_UOP cells (excluding CONST, which we render per-reference
as leaves) now key the wire on the producer's base
(`uop<val>` instead of `w<loc>`), so all consumer slots and the
producer's output share one wire.  DC then draws a spider where
the producer fans out to all the consumers -- same idiom we
already use for VAR / DP0 / DP1.

`plainUopDiagram` and `gradDiagram` updated their synthetic-fallback
pWire (used when the seed is heapless) to match the new naming
(`uop<base>` instead of `p<base>`).

### Fixed: grad chain rule allocates fresh EXPAND per branch + single-line node headers

`grad_rec` previously lifted `gy` to target.shape ONCE in
`interact_grad`, which was correct numerically but produced a heap
where multiple chain-rule consumers all referenced the same EXPAND
node.  In any visualisation that doesn't fan out via DUP, all but
one of those consumer wires dangled (visible in `grad-x-times-x`:
the second branch's MUL had a missing CONST input).

Fix: each branching chain-rule node (`UOP_MUL`, `UOP_ADD`,
`UOP_NEG`, `UOP_REDUCE`) now allocates a *fresh* EXPAND of `gy`
per branch.  A new `gy_lifted` flag threaded through `grad_rec`
prevents redundant outer EXPANDs at deeper leaf positions when
the cotangent is already target-shaped.  Test structure update
in `tests/test_grad.c`; numerics unchanged (9/9 WL grad cases
still pass).

### Changed: single-line node headers carry heap loc + handle id

Diagram + heap-graph labels now use `OPCODE@<heap-loc>(#<id>)` on
one line instead of stacking opcode and base across two lines.
The `#<id>` suffix only appears when the opcode carries an extra
handle: `KERNEL@10#2` (kernel id from the `NUM(kid)` cell),
`GRAD@3#1` (target tensor id from cell base+2), `TEN@10#1` (cell
loc + tensor id).  Plain compute UOPs stay terse: `MUL@8`,
`ADD@14`.  Shape (when known) and CONST scalar value remain on
follow-up lines.

### Changed: WL kernel split into per-concern files; shape inference centralised

`wl/THVMLink/Kernel/` now uses one BeginPackage["THVMLink`"] +
Begin["`Private`"] block per file, all sharing the same private
context.  Cross-file references resolve directly without
THVMLink`Private`-qualified calls.

Two new files separate concerns that used to be inlined in the
renderers:
- `Shape.wl` -- shape arithmetic (`broadcastShape`, `dropAxis`,
  `shapeText`), tensor-id shape lookup (`tenShapeOf`), and the
  manual IEEE 754 single-precision decoder (`bitsToReal32`,
  `bitsToInt32`, `scalarTextFromCell`).
- `Uop.wl` -- per-opcode metadata in one place: `uopArity`,
  `uopName`, plus an inferred-output-shape walker (`uopShapeOf`,
  `cellShape`, `uopSrcShape`) that mirrors the rules in
  `src/schedule/materialize.c`.

`Visualization.wl` and `Diagram.wl` now read these helpers
directly, dropping their duplicated tables.  UOP labels gained the
inferred output shape (e.g. "MUL\n@8\n{3}") and CONST keeps both
its heap base and its scalar value.

`THVMLink.wl` no longer hard-codes the load order -- after its
own EndPackage it Get's every other `*.wl` in the Kernel directory
in alphabetical order.  Adding a new sibling file means dropping
it in; no edits to the loader.

### Changed: shape-aware grad_rec drops the MUL(target, CONST(0)) wrapper

`interact_grad` no longer post-wraps the chain-rule output in
`ADD[raw, MUL(target, CONST(0))]` to coax materialize into producing
target-shaped gradients.  Instead, every leaf-level emission inside
`grad_rec` is wrapped in `EXPAND(_, target.shape)`:

- leaf match (`y === target`)        -> `EXPAND(gy, target.shape)`
- independent leaf / NUM             -> `EXPAND(CONST(0), target.shape)`
- `UOP_CONST`                        -> `EXPAND(CONST(0), target.shape)`
- `default`                          -> `EXPAND(CONST(0), target.shape)`

This required minimal materialize + interpret support for `UOP_EXPAND`
(previously a step-14 placeholder): `op_output_shape` now reads the
heap NUM cells for EXPAND's target dims (using the source view's rank
to know how many cells to read -- tinygrad EXPAND preserves rank), and
a new `cpu_op_expand` fans the source buffer out to the larger numel.
Sufficient for the autograd path (scalar -> 1-D); per-axis broadcast
in higher ranks lands with view tracking in step 14.

`tests/test_grad.c` was rewritten to expect the EXPAND wrapping
(replacing the old `unwrap` helper that stripped the dead `MUL` wrapper).
WL `grad.wlt` end-to-end numerics still pass (9/9).

### Added: shape on TEN labels, scalar value on CONST labels

`THeapDiagram`'s leaf labels now carry the data the user actually wants
to see:
- `TEN#<id>` -> reads `TENS[id].view.shape` and shows e.g. `{3}` on a
  third line.
- `CONST` -> decodes the NUM cell's bits via manual IEEE 754 (so a
  CONST(1.0) renders as `CONST\n1.` instead of a mystery `CONST\n@2`).

### Added: tensor-aware THeapDiagram (IC string-diagram path)

Diagram.wl now renders TAG_UOP / TAG_TEN terms via Wolfram`Diagrammatic`-
`Computation`, so `THeapDiagram[term]` produces a proper IC string
diagram for tensor compute graphs (it previously returned an empty
network for anything that wasn't pure IC).

UOP rendering uses an opcode-driven shape/style:
- Plain compute UOPs (ADD/MUL/...) -- apex-down blue triangle, N
  inputs at top (one per `uopArity[opcode]`), 1 output at bottom.
- GRAD -- DUP-shaped (apex-up orange triangle), 1 input at top
  apex (the y branch), 2 outputs at the flat bottom (forward
  passthrough + backward gradient).  `gy` and `target` cells are
  hidden; the target tensor id is surfaced as `#<tid>` in the
  GRAD label.

TEN handles render as cyan apex-down triangles, one leaf per
referencing slot (no DUP needed for multi-reference -- each ref
gets its own `TEN#<id>` triangle).

CONST UOPs (zero-arity) are rendered the same way: per-reference
leaves labeled `CONST@<base>`, so a constant referenced from N
slots draws N triangles instead of forcing a shared agent (which
would require DUPs to fan out).

Reachability filter walks UOPs/TENs forward from the seed term
so post-`TWnf` heaps don't surface their pre-rewrite cells.
`principalCellOf` was tightened to consider only cells inside
reachable agents' slot ranges, so dead heap can't grab a UOP's
output wire.

`run.wls` no longer skips IC diagrams for `grad-` examples; both
pre-reduce (`diagram.png`) and post-WNF (`diagram-wnf.png`) IC
diagrams are now rendered alongside the heap graphs.

New plain-UOP examples (no grad rewrite):
- `wl/Examples/uop-add` -- `TUOpAdd[a, b]`
- `wl/Examples/uop-mul` -- `TUOpMul[a, b]`
- `wl/Examples/uop-mul-add` -- `(a*b)+c`

Existing grad-`*` examples now use lazy `TTensor[{3}]` allocations
instead of `TTensorCreate @ NumericArray[...]`; the visualization
doesn't need real numerics and the lazy form is shorter.

### Added: tensor-aware heap graph + grad- visualization examples

`Visualization.wl` got a major extension to render tensor compute
graphs (it previously only knew about IC tags LAM/APP/SUP/DUP/ERA,
so any `TAG_UOP` / `TAG_TEN` term came out blank).

New vertex-id convention prefixes the kind:
- `a<base>` -- IC compound at args base `<base>`
- `e<loc>`  -- ERA cell at heap loc
- `u<loc>`  -- TAG_UOP at heap loc
- `t<id>`   -- TAG_TEN at tensor id

Per-tag rendering:
- `TAG_TEN` -- cyan square labeled `TEN\n#<id>`
- `TAG_UOP` -- blue rectangle labeled `<OPCODE>\n@<loc>`
- Edge labels follow `src<N>` using a per-opcode `uopComputeArity`
  table (NUM-only cells stay implicit).

Single-vertex default size bumped (0.18 -> 0.45) so identity-only
terms don't render as a pinhead.

Three new `wl/Examples/grad-*` folders, each with `term.wl` plus
pre-reduce (`term.png`) and post-`TWnf` (`term-wnf.png`) heap
renderings:
- `grad-add` -- gradient of `a + b` w.r.t. `a` -> ones_like(a)
- `grad-mul` -- product rule `d(ab)/da` -> b
- `grad-x-times-x` -- `d(a*a)/da` -> 2a

`run.wls` detects `grad-`-prefixed folders, skips the IC string
diagram (tensor graphs aren't IC nets), and renders both the
pre-reduce graph and the post-`TWnf` rewritten graph using the
`TWnf` result as the discovery seed.

### Added: PLAN.md step 13 (partial) -- UOP_GRAD reverse-mode autograd

`UOP_GRAD` is the 18th UOp opcode and a pure rewrite rule (not a
graph node that survives reduction).  Reducing
`UOP_GRAD[y, gy_seed, target]` under `TWnf` recursively applies the
chain rule until no `UOP_GRAD` nodes remain, then wraps the result
in a `target * 0` summand so the broadcast machinery in materialize
projects it onto target's shape.

Step-13 chain-rule coverage: leaf cases (target match, other tensor,
NUM, CONST), `UOP_ADD`, `UOP_MUL` (product rule), `UOP_NEG`, and
`UOP_REDUCE` (SUM only -- MAX needs an indicator one-hot, deferred
to step 14).  Anything else returns `CONST(0)` and warns.

WL surface:
- `TUOpGrad[y, gy, target]` -- explicit cotangent.
- `TGrad[y, target]` -- top-level VJP shortcut with `gy = CONST(1)`.

`materialize_expr` recognises `UOP_GRAD` and reduces it inline before
kernelizing, so `TMaterialize[TGrad[...]]` works without a separate
TWnf pass.

Tests:
- `tests/test_grad.c` (16 checks): structural pin-downs of the
  rewrite output for each handled opcode.
- `wl/THVMLink/Tests/grad.wlt` (9 checks): end-to-end f32 numerics
  including identity, independent leaf, ADD, MUL product rule,
  NEG, REDUCE_SUM broadcast-back, `x*x = 2x`, and `2x + 3 = 2`.

### Removed: `Function[t_TTerm]` UpValue

The `(f_Function)[t_TTerm] -> TApp[TLam[f], t]` IC sugar was a
footgun -- it silently rewrote any pure-function map over TTerms
into a beta-redex (which surfaced as a crash when our numeric
Plus/Times UpValues used `& /@`).  Removed alongside the
`$inTLamBinder` guard that only existed to break the resulting
recursion.  `TTerm[id_Integer][arg]` sugar (forming
TApp[TTerm[id], arg]) stays.

### Added: PLAN.md step 12 -- TTensor + TUOp + materialize + dispatch

End-to-end tensor pipeline.  WL-built UOp graphs reduce naturally
through schedule + kernelize + linearize + interpreter dispatch to
concrete `TAG_TEN` results, all under one `TWnf` call.  See
`docs/tensors.md` and `docs/glossary.md`.

Six commits across the step:

- **tensor foundation** (139af93)
  - Three new term tags: `TAG_TEN` (8) atom for tensor handles,
    `TAG_UOP` (9) heap-backed for graph nodes, `TAG_NUM` (10) atom
    for inline scalars.
  - `TenDesc` side table (`TENS[]`) with refcount, View
    (shape/strides/offset), buffer id, and Backend pointer.
  - CPU `Backend` vtable: alloc/free/incref/decref + buf_read/write,
    parallel `CPU_BUFS[]` table with its own refcount for view
    aliasing.
  - View aliasing (`tensor_view_of`) bumps the buffer refcount so
    reshape/permute can share storage zero-copy in step 14.

- **UOp vocabulary + WL surface** (719ac4a)
  - 18 opcodes covering CONST, six movement ops
    (RESHAPE/PERMUTE/EXPAND/PAD/SHRINK/FLIP), eight elementwise ops
    (ADD/MUL/NEG/RECIP/EXP2/LOG2/SQRT/CMPLT), REDUCE, plus the
    rewrite triggers MATERIALIZE and KERNEL.
  - One `src/uop/<op>.c` per opcode emitting the documented heap
    layout.
  - WL surface in the new `Tensor.wl` sibling: `TTensor`,
    `TUOpAdd/Mul/.../Reduce`, `TUOpMaterialize`, plus inspection
    helpers.

- **TRealize + TTensorCreate + zero-copy NumericArray I/O** (862887e)
  - `TRealize[expr] := TWnf[TUOpMaterialize[expr]]`.
  - `TTensorCreate[data]` shares a `NumericArray`'s buffer on the
    CPU backend (Shared passing mode + per-buffer cleanup
    callback).  PackedArrays / nested Lists lift to NumericArray
    first.
  - `TTensorData` returns a `NumericArray` whose type matches the
    dtype (single memcpy in the f32 fast path; no f32 -> f64
    conversion).
  - CpuBuf gains `owns_data` + `on_release` callback so the same
    slot can hold either malloc'd or borrowed bytes.

- **materialize pipeline** (8ffd333)
  - New `KERNELS[]` side table with linearized `KProgOp` programs;
    the same SSA-over-indices shape tinygrad's PYTHON device
    consumes.
  - `src/schedule/materialize.c` rewrites a UOp graph into a tree
    of `UOP_KERNEL[output_buf, NUM(kid)]` terms; recursively
    materializes children, dedups identical inputs.
  - `TMaterialize` WL helper for inspecting the scheduled DAG
    *before* kernel firing; `TKernelInfo[kid]` returns the
    linearized program as an Association.

- **CPU interpreter + interact_kernel** (3e071bd)
  - Per-op CPU files under `src/backend/cpu/op/` (one per opcode,
    matching the project's file = function name convention).
  - `cpu_interpret` walks `KernelEntry.program[]`, allocates one
    scratch per intermediate, dispatches via switch on opcode.
  - `interact_kernel` recursively fires producer kernels first
    (via the new `TenDesc.producer_kid` field), then invokes
    `Backend.dispatch_kernel` for the current kernel.  Increments
    `ITRS` once per firing, the same way HVM4 counts an OP2-NUM-NUM
    collapse.
  - `wnf` extension: `TAG_UOP/UOP_MATERIALIZE` -> direct rewrite,
    `TAG_UOP/UOP_KERNEL` -> fire, anything else -> WNF.

- **PLAN.md** (9b5a4db)
  - Step 12 marked done.

Numerical UpValues on `TTerm` (Plus / Times / Minus / Power[1/2] /
Less) rewrite ordinary WL arithmetic against tensor-shaped TTerms
into UOp graphs.  Scalars lift to UOP_CONST with the seed tensor's
dtype.

Removed: the `Function[t_TTerm]` UpValue that converted `f[t]` to
`TApp[TLam[f], t]`.  It was dumb, surprised the Plus/Times rewrite
that maps over tensors, and the matching `TLam[$inTLamBinder] guard`
went with it.

End-to-end:
```mathematica
a   = TTensor[{4}, {1.0, 2.0, 3.0, 4.0}];
b   = TTensor[{4}, {10.0, 20.0, 30.0, 40.0}];
out = TRealize[2.0 * (a + b) + 1.0];
Normal @ TTensorData[out]
(* {23.0, 45.0, 67.0, 89.0} *)
```

### Added: THeapDiagram (Wolfram`DiagrammaticComputation` backend)

- New `wl/THVMLink/Kernel/Diagram.wl` subpackage at context
  `THVMLink`Diagram` exporting `THeapDiagram[term]`, which builds a
  `DiagramNetwork` from the current heap using the
  `Wolfram/DiagrammaticComputation` paclet (assumed installed).
- The subpackage lives in its own context so its `BeginPackage`
  imports can pull in `Wolfram`DiagrammaticComputation` and its
  `Diagram` subcontext without shadowing names in the main
  `THVMLink` context.
- Wire-name strings are unique per heap location: `w<loc>` for
  cells, with `VAR` cells collapsed to their binder's wire and
  `DP0`/`DP1` cells expanded to `dup<base>_dp{0,1}_lab<ext>`.
- `wl/Examples/run.wls` now writes `diagram.png` next to `term.png`
  for each example's input `term.wl` (skipped for reference
  variants like `term-reduced.wl`).

### Added: TTermExpr / TTermTree, TReduce, .hvm refs, restructured examples

- `wl/Examples/` folders no longer carry numeric prefixes; the
  reduced variants merge into their parent (`02-id-app-era` +
  `03-id-app-era-reduced` -> `id-app-era/` with both `term.wl` and
  `term-reduced.wl`).  `13-church-2-applied` becomes its own
  top-level `church-2-applied/` (the lambda lives in `church-2/`).
- `term.wl` is the input term construction.
- `term-reduced.wl` (optional) constructs the *expected* WHNF
  directly -- no `TWnf` / `TReduce` inside it.  The reduction test
  runner compares `TTermExpr[TWnf[term.wl]]` against
  `TTermExpr[term-reduced.wl]`.
- `term.hvm` (optional) carries the HVM4 surface-syntax reference
  with the expected output as a `//` comment line.  Documentation
  only; we have no parser yet.
- `wl/Examples/run.wls` now scans `term*.wl` per folder but only
  renders the input `term.wl` (skips `term-reduced.wl`, which is
  reference data).
- `wl/Examples/test_reductions.wls` is the reduction-comparison test
  driver, wired up as `make wl-examples-test`.
- New WL helpers in the paclet:
  - `TReduce[t]` = `(TWnf[t]; t)` -- reduces in place and returns the
    original root.  Useful as a `THeapGraph` seed when you want to
    visualise the post-reduction state.
  - `TTermExpr[t]` walks the heap from `t` and returns a nested
    expression with tag-name string heads (`"LAM"`, `"APP"`, `"SUP"`,
    `"DUP"`, `"DP0"`, `"DP1"`, `"VAR"`, `"ERA"`).  Cycles produce
    `"Cycle"[loc]` leaves.
  - `TTermTree[t]` = `ExpressionTree[TTermExpr[t]]` for visual
    rendering as a Wolfram `Tree`.
- README catalogue rewritten with the new folder names + an
  "Expected WHNF" column pointing at `term-reduced.wl`.

### Added: dark export + auto-fit labels + sugar

- WL `THeapGraph` accepts trailing `Graph` options via
  `OptionsPattern[]` (per the GUIDE) so callers can override
  `GraphLayout`, `VertexSize`, `PlotRange`, `Background`, etc.
- Vertex labels now render INSIDE each shape via `Inset[Pane[label,
  {pixelW, pixelH}, ImageSizeAction -> "ShrinkToFit"]]`.  Labels
  auto-shrink so the same `LAM @0` text fits cleanly in any vertex
  size.
- `VertexShapeFunction` honours the `size` argument throughout,
  including the ERA stroked Circle, so `VertexSize -> Tiny | Small |
  Large | Scaled[...]` all behave.  Removed the manual
  `singleVertexLoopFn` hack -- the default Wolfram self-loop renderer
  works once the shape sizes are scaled correctly and the plot range
  has room for the loop (single-vertex case explicitly widens
  `PlotRange` and shrinks the vertex).
- Examples export onto a dark `GrayLevel[0.12]` background with
  `Style[..., "DarkScheme"]` so `LightDarkSwitched` picks the
  dark-mode arm (white labels, darker fills, white outlines).
  Generated PNGs now read cleanly on dark READMEs and notebooks.

### Added: TTerm sugar (call as function, lambda literal)

- `TTerm[id_Integer][arg_]` desugars to `TApp[TTerm[id], arg]` so
  users can write `id[era]` instead of `TApp[id, era]`.
- `(var |-> body)[t_TTerm]` desugars to `TApp[TLam[var |-> body], t]`
  via a tagged UpValue on `TTerm`.  Lets you write a literal
  beta-redex without spelling out `TLam` / `TApp`.
- The `Function` UpValue is guarded by `$inTLamBinder` so `TLam`'s
  own internal call `builder[TVarFor[loc]]` does not trigger it
  (which would recurse infinitely).
- Two new VerificationTests cover both forms.

### Added: DUP-LAM + church-numeral examples

- `src/interact/dup_lam.c`: real DUP-LAM rule.  Allocates one
  five-cell block holding the new pair of bound vars (as a SUP
  inside the original binder) and the new pair of body projections
  (as a fresh DUP over the original body).  No body cloning happens
  eagerly -- only when a future projection inspects part of the
  body does it descend lazily.  This is the rule that gives Church
  numerals (and similarly Lamping / optimal-reduction style
  workloads) their non-exponential cloning behaviour.
- `tests/test_dup_lam.c`: two C tests; clone an identity lambda and
  confirm DUP-LAM fires once, then end-to-end apply one of the
  cloned copies to ERA.
- `wl/Examples/10-k-combinator/`, `11-church-1/`, `12-church-2/`,
  `13-church-2-applied/`: four new runnable examples.  The Church 2
  family exercises the DUP machinery; the applied form reduces
  end-to-end and the resulting graph (in `13-...-applied/graph.png`)
  shows the post-firing heap including the cloned lambdas and the
  substituted DUP cell.
- Two new VerificationTests in `wl/THVMLink/Tests/core.wlt`: a
  direct DUP-LAM clone, and Church-2-applied reducing to the
  identity-applied result.
- `docs/interact/dup_lam.md` documents the rule, the C, the cost,
  and why the lazy-cloning shape matters.

### Added: visualization renderer split + theme-aware colors

- `wl/THVMLink/Kernel/Visualization.wl`: extracts the heap-graph
  renderer into a dedicated kernel sibling.  THVMLink.wl now `Get`s
  it after declaring public symbols.
- Theme-aware colors throughout: `LightDarkSwitched[Black, White]`
  for foreground; `Lighter[StandardX, 0.55]` / `Darker[StandardX,
  0.45]` per-tag agent fills (green LAM, blue APP, orange SUP,
  purple DUP); ERA stays as a plain foreground-stroked Circle.
- Vertex labels now render in column form: `TAG\n@<base>` for
  arity-1 agents, `TAG\n@<base>..<base+1>` for arity-2.
- Triangles are real triangles via `Triangle[]` (not trapezoids)
  with apex orientation matching IC convention: LAM/DUP point down,
  APP/SUP point up.
- VertexShapeFunction now respects the size argument so
  `VertexSize -> Tiny | Small | Large | Scaled[...]` actually take
  effect.
- Single-vertex self-loop is drawn explicitly via
  EdgeShapeFunction; the identity lambda's loop is now visible.
- Pink "background" mystery solved: `Dashing[{Small, Small}]` was
  invalid (Small is not a numeric Dashing arg) which silently put
  Wolfram into an error-overlay state.  Replaced with the proper
  `Dashed` directive.
- Context-shadowing fix: switched `wl/Examples/run.wls` and
  `wl/THVMLink/Tests/run.wls` from `Needs["THVMLink`"]` to
  `Get["THVMLink`"]` so user code resolves to package symbols
  rather than auto-created `Global`*` placeholders.
- `wl/GUIDE.md` gains a Dark-mode + Standard colors section and an
  OptionsPattern[] section.

### Added: TTerm atomic wrapper + ensureInit

- TTerm[id_Integer] is the canonical wrapper around a packed Term;
  TLam / TApp / TSup / TDup / TEra / TVarFor return TTerm-wrapped
  values; TTermTag/Ext/Val/Sub accept either a TTerm or a raw
  Integer.  Old TTermInfo is gone (folded into TTerm[id]["info"]);
  TTermNew is no longer in the public API (private packTerm helper).
- TTerm[id]["tag" | "ext" | "val" | "sub" | "tagName" | "raw" |
  "info"] forwards to the bridge.  Format.wl gives TTerm a summary
  box keyed off the structural pattern (QuantumFramework style).
- ensureInit[]: heap-touching ops auto-call TInit if the runtime is
  not initialised yet.  TFree clears the flag.

### Added: wl/Examples/ runnable example database

- New `wl/Examples/` directory: one folder per example term, each
  holding a minimal `term.wl` (no `Needs`, no `TInit`, just the
  expression to construct the term) plus the rendered
  `graph.png` produced by the runner.
- 9 examples covering every interaction we currently fire: identity
  lambda, (id ERA) before / after `TWnf`, (ERA lam) before / after
  `TWnf`, bare `TSup[ERA, ERA]`, DUP-SUP same-label annihilation
  before / after, and nested APPs.
- `wl/Examples/run.wls`: single CLI for both bulk and per-example
  runs.  Loads the paclet, calls `TInit` per example, evaluates the
  `term.wl`, exports the resulting `THeapGraph[term]` as a PNG
  alongside the source.  Supports a positional example id and a
  `--eval` flag to skip the PNG export.
- `wl/Examples/README.md` catalogues every example and documents how
  to add new ones.
- `make wl-examples` (regenerate every PNG) and
  `make wl-examples EXAMPLE=<id>` (just one).
- `docs/heap_graph.md` now embeds two of those PNGs directly from
  `wl/Examples/<id>/graph.png` so the doc and the runnable example
  stay in sync.  The previous one-off `docs/images/` directory is
  removed.
- `wl/GUIDE.md` gains a rule for multi-line `If`: leading space after
  the bracket so the test argument lines up with the branches
  (`If[ cond, then, else]`).

### Added: heap graph rendering (PLAN.md step 10)

- `THeapGraph[]` and `THeapGraph[term]` (or `THeapGraph[{t1, t2,
  ...}]`) render the runtime as an IC string-diagram Wolfram
  `Graph[]`: compound terms (LAM/APP/SUP/DUP) are agent vertices
  keyed by their args base, VAR cells collapse into wires labelled
  `var`, and ERA cells render as small black dots.  Optional seed
  terms add agents that are heapless (held only as WL return values).
- `THeap[]` now returns an atomic `THeap[<|nextLoc, cells, Graph|>]`
  with the rendered graph at the `"Graph"` key (capitalized).
- `TTermInfo[t]` now returns an atomic `TTermInfo[<|...|>]` with the
  same payload shape.
- Both atomic objects expose Association-style indexing via DownValues
  and forward `KeyExistsQ`/`Keys`/`Values`/`Normal` via UpValues so
  callers see the same access shape as before.
- `wl/THVMLink/Kernel/Format.wl` defines the `MakeBoxes` UpValues
  (QuantumFramework-style: structural Q-test guarded by `Unevaluated`,
  `BoxForm`ArrangeSummaryBox` for the visual).  Loaded from
  `THVMLink.wl` after the public symbols are declared.
- `TFreshLabel[]` returns a fresh integer from a monotonic counter
  (reset by `TReset[]`).  `TSup[a, b]` and `TDup[body, k]` now
  auto-label via `TFreshLabel[]`; the existing 3-arg
  `TSup[label, a, b]` / `TDup[label, body, k]` forms remain for
  tests that need explicit label matching.
- `wl/THVMLink/Tests/core.wlt` gains six new VerificationTests:
  fresh-label monotonicity + TReset rewind, auto-label distinctness
  for both SUP and DUP, identity-lambda `THeapGraph` shape, seeded
  vs unseeded graph for `TApp[id, ERA]` and `TDup[TSup[ERA, ERA], k]`.
- `docs/heap_graph.md` is now the permanent reference for the model
  (agent-as-vertex, VAR-as-wire, ERA-as-dot) with six worked
  snapshots, mermaid diagrams, and live `THeapGraph` PNGs for
  examples 2 and 4 (regenerated by `docs/images/generate.wls`).
- `docs/term.md` gains a glossary table pinning down term / cell /
  loc / slot / agent / args base / port / node / wire and links
  forward to `docs/heap_graph.md`.
- `docs/wl.md` documents the `Format.wl` summary-box layer and the
  layout convention.
- `docs/images/generate.wls` produces the PNGs embedded in the doc;
  `docs/images/` is the canonical location for generated diagrams.

`make test`    -> 91 C checks pass.
`make wl-test` -> 23 WL VerificationTests pass.

### Added: architecture docs (PLAN.md steps 8-9)

- `docs/` with a self-contained markdown per piece, indexed by
  `docs/README.md`:
  - `docs/term.md`: bit layout + tag table + worked examples.
  - `docs/heap.md`: bump allocator + the substitution model
    (`heap_subst_var`, `heap_subst_cop`).
  - `docs/wnf.md`: enter/apply state machine, frame protocol, and
    the dispatch table for current interactions.
  - `docs/interact/_.md`: index of active-pair rules + tracking of
    which active pairs are stuck (deferred).
  - `docs/interact/{app_lam,app_era,dup_sup,dup_era}.md`: one page
    per interaction with the sequent rule, the C, a worked example,
    and a cost summary.
  - `docs/wl.md`: WL paclet design (scalar bridge + WL-side
    constructors) and usage.
- `README.md`: top of the file points at `docs/README.md` and the
  layout block now includes `docs/`.
- `AGENTS.md`: workflow step 4 clarifies that the `docs/interact/`
  page is the source of truth when it disagrees with the C file's
  header comment.

### Added: minimal reducer + interactions (PLAN.md steps 5-6)

- `src/wnf/_.c`: real two-phase stack-machine reducer (enter/apply)
  modeled on HVM4's clang/wnf/_.c.  Pushes APP / DP0 / DP1 frames at
  enter, dispatches active-pair interactions at apply, rebuilds stuck
  nodes by writing the reduced head back into the heap cell.
- `src/interact/app_lam.c`: real APP-LAM beta (`(lam x.body) arg`
  substitutes `arg` at the binder loc and continues into `body`).
- `src/interact/app_era.c`: APP-ERA (erased function yields ERA).
- `src/interact/dup_sup.c`: DUP-SUP same-label annihilation.  The
  commuting (different-label) case is left stuck for now; a test will
  drive the implementation when needed.
- `src/interact/dup_era.c`: DUP-ERA (both projections receive ERA via
  `heap_subst_cop`).
- `src/heap/subst_cop.c`: pair-substitution helper used by both
  DUP-style interactions; substitutes one side and returns the other.
- `src/thvm.h`: declares the new `interact_*` and `heap_subst_cop`
  signatures.
- `tests/test_app_lam.c`, `tests/test_era.c`, `tests/test_dup_sup.c`:
  removed the `PENDING(...)` gates and added ITRS-counter assertions
  so each test verifies the specific interaction fires (and only
  fires once).
- `wl/THVMLink/Tests/core.wlt`: three new VerificationTests covering
  APP-LAM, APP-ERA, and same-label DUP-SUP through the LibraryLink
  bridge.
- `Makefile`: moved `SRC :=` definition above the `$(WL_LIB)` rule so
  `make wl` correctly retriggers when any C runtime file changes.

`make test` -> 91 C checks pass.  `make wl-test` -> 14 WL tests pass.

### Added: WL paclet (PLAN.md step 4)

- `wl/THVMLink/` paclet that exposes the C runtime to Wolfram
  Language, with the LibraryLink bridge in `wl/THVMLink/CSource/`,
  the package in `wl/THVMLink/Kernel/THVMLink.wl`, and tests in
  `wl/THVMLink/Tests/`.
- `wl/THVMLink/CSource/thvmlink.c` exports 14 scalar `EXTERN_C
  DLLEXPORT` functions covering lifecycle (init/free/reset), term
  packing/unpacking (`thvm_wl_term_*`), heap access
  (`thvm_wl_heap_pos/alloc/read/set`), the WNF entry point, and the
  interaction counter. Every function is scalar-in / scalar-out (no
  arrays, no opaque handles).
- `wl/THVMLink/Kernel/THVMLink.wl` synthesizes higher-level term
  constructors (`TLam`, `TApp`, `TSup`, `TDup`) from the scalar
  primitives via shared `heapWith` / `heapTerm` helpers, plus the
  inspector `TTermInfo` and the heap snapshot `THeap[]`.
- `wl/THVMLink/Tests/core.wlt` defines 11 `VerificationTest` specs
  covering term packing roundtrip, heap primitives, the four
  high-level constructors, the heap snapshot, and the WNF stub
  passthrough.
- `wl/THVMLink/Tests/run.wls` is the test runner. It loads the
  paclet, invokes `TestReport` on every `*.wlt` file, prints
  `wl tests: N passed, M failed` to stdout, lists each failed test,
  and exits non-zero on any failure.
- `wl/GUIDE.md` records WL style rules: no `Print` (use a local
  `debugPrint` wrapping `WriteString`), no em dashes, no Unicode
  box-drawing characters, no decorative arrows in source.
- `Makefile` gains `make wl` (build the dylib at
  `wl/THVMLink/LibraryResources/$(WL_PLATFORM)/THVMLink.dylib`) and
  `make wl-test` (run `run.wls`). Auto-detects the newest
  `/Applications/Wolfram*.app`; override with
  `WOLFRAM_APP=/Applications/Wolfram\ X.Y.app`.

### Added: scaffold (PLAN.md steps 0-3)

- `AGENTS.md` with conventions (path-is-the-function-name, single TU,
  one-interaction-per-file), build/test instructions, and a code map.
- `.gitignore` covering `bin/`, `*.o`, `*.dylib`, macOS `.DS_Store`,
  the local-only `.claude/` settings dir, and the `TinyHVM` reference
  symlink.
- `Makefile` with `make` (build all), `make test` (build + run tests),
  `make clean`. Tests are independent C programs that include
  `src/thvm.c`.
- `src/thvm.h` declaring the term bit layout (SUB:1 / TAG:7 / EXT:18 /
  VAL:38), the minimal tag set (APP, LAM, VAR, ERA, DP0, DP1, SUP,
  DUP), heap globals, and function signatures for the
  term/heap/wnf/interact modules.
- `src/thvm.c` single-TU hub that `#include`s all `.c` files in build
  order.
- `src/term/{new,tag,ext,val}.c` and `src/term/sub/{get,set}.c`:
  full implementations of term packing/unpacking. Trivial
  bit-twiddling.
- `src/heap/{alloc,read,set,take,subst_var}.c`: flat single-threaded
  bump-allocated heap with substitution helper.
- `src/wnf/_.c`: WNF stack machine **stub** that returns its input
  unchanged. Step 6 will replace this with the real reducer.
- `src/interact/app_lam.c`: APP-LAM beta reduction **stub**. Step 6
  fills it in.
- `tests/test_term.c`: round-trip test for `term_new` and
  `term_tag/ext/val/sub_get`. 73 checks pass.
- `tests/test_heap.c`: alloc-then-read-back, set-then-read-back. 9
  checks pass.
- `tests/test_app_lam.c`, `tests/test_era.c`, `tests/test_dup_sup.c`:
  carry the spec for APP-LAM beta, ERA propagation, and DUP-SUP
  collapse/commute. Bodies are gated by `PENDING(...)` until step 6
  lands `wnf` and the interactions, so they exit 0 today and report
  `pend` in `make test` output.
- `README.md` describing what works today and what is stubbed.
