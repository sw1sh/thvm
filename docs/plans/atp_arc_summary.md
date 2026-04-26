# IC-native ATP arc: closing summary (stage 8.10c)

> Final memo of the IC-native ATP arc.  Recaps what shipped
> across stages 1-8.10, what stayed deferred, and what the
> natural follow-on stages are.  The arc itself spans 79
> `feat:` commits + a dozen `task: decompose/block/unblock`
> commits.

## What shipped

### Stages 1-4: foundations

The first four stages built the core saturation engine in C
following Waldmeister's `Hauptkomponenten` ("main components")
loop:

- **Stage 1**: tag inventory expanded (`TAG_EQL`, `TAG_AND`,
  `TAG_OR`, `TAG_ANY`, `TAG_INC`, `TAG_CTR`, `TAG_WHEN`,
  `TAG_FVR`, `TAG_BRI`, `TAG_ANN`).  These provide the
  vocabulary that 5-8 wire together.
- **Stage 2**: KBO comparator (`src/kbo/_.c`) with
  Baader-Nipkow's algorithm; first-order term encoding via
  TAG_CTR + TAG_FVR.
- **Stage 3**: rewrite engine (`src/rewrite/_.c`) with
  recursive descent; `thvm_match` / `thvm_subst_apply` /
  `thvm_rewrite_step` / `thvm_rewrite_normalize`.
- **Stage 4**: critical-pair generator (`src/cp/_.c`) with
  Robinson MGU (`src/unify/_.c`); position-walk overlap
  enumeration.

### Stage 5: saturation loop

`src/atp/_.c` glued it together: AtpState struct, CP queue,
priority-aware selection via INC + collapse_ordered, full
6-step Knuth-Bendix completion loop with unfailing fallback.

### Stage 6: spec parser + PCL trace

Top-level `wald_parse_file(path, spec)` reads Waldmeister
`.pr` syntax: NAME, MODE, SORTS, SIGNATURE, ORDERING, VARIABLES,
EQUATIONS, CONCLUSION (later: EXISTS).  PCL-shaped trace
serializer for proof-tree introspection.  End-to-end
`example.pr` saturation runs at the C level.

### Stage 7: redundancy criteria

Five criteria, with empirical analysis of each:

- **7.1 trivial-joinability filter** (`atp_cp_trivially_joinable`):
  drops CPs that normalize to the same term under R; ~52% of
  generated CPs on the group example.
- **7.2 connectedness redundancy**: design memo proved BDP
  connectedness is **dominated by 7.1** in our codebase
  (rule-subset joinability cannot find joins R can't); 7.2b
  ships an empirical-only counter.
- **7.3a rule-subsumption counter**: similarly dominated;
  shipped as a counter for empirical confirmation.
- **7.3b queue-subsumption filter**: genuinely orthogonal to
  7.1 (queue doesn't participate in normalization); real
  pruning.
- **7.4 Twee comparison bench**: `tools/bench_twee.c` runs
  Twee 2.6.1 (installed via `cabal install twee`) on the same
  fixtures.  Twee proves all 4 fixtures in 24-30 ms each,
  including our TIMEOUT case; we win easy-goal latency by
  3-4 OOM (no process spawn / TPTP parse) but lose hard-
  saturation throughput.

### Stage 8: full-fledged ATP iteration

Ten sub-stages, mostly research-grade:

- **8.1 SUP-encoded CP enumeration via TAG_PRI** (a-e):
  `prim_unify_apply` / `prim_unify_apply3` / `atp_ic_rewrite_*`.
  IC-routed CP enumeration produces byte-identical outputs to
  the C path; bench shows IC within run-to-run noise on the
  TIMEOUT case.  `use_ic_cp_gen` flag opt-in.
- **8.2 KBO as a pure IC program** (a, b, c; d blocked):
  `prim_kbo` (TAG_PRI wrapper) + `prim_kbo_eq_ic` (pure-IC
  structural-equality via AND-chain self-recursion).  Full
  pure-IC `thvm_kbo` blocked pending SupGen use case.
- **8.3 IC-native rule dispatch** (a, b, c, e; d closed via
  resolution memo): `prim_rewrite_step` LibraryLink primitive
  + SUP-of-rules fan-out demo + `use_ic_rewrite` flag.
  ICC TAG_BRI/TAG_ANN integration resolved as "subsumed by 8.4
  sort-check gating."
- **8.3e cross-product bench** (`cc/ci/ic/ii` modes): all 4
  modes byte-identical counters; IC-rewrite ~2x slower than C
  on TIMEOUT case (within target).  Heap-overflow finding:
  IC-rewrite at budget=256 overflows HEAP_CAP, so bench reduced
  to budget=32.
- **8.4 multi-sort signatures** (a-e): WaldSpec sort table +
  per-symbol/per-variable sort indices + `wald_sort_check`
  precheck + saturation-entry-point gating + `nat_list.pr`
  fixture.  Closed 8.3d's deferral.
- **8.5 LPO ordering** (a-d): `thvm_lpo` Lexicographic Path
  Ordering (Dershowitz 1982) + `LpoConfig` + `s->lpo` field +
  bench harness dispatch on `spec->ordering_kind`.
  **Empirical finding**: KBO and LPO orient identically on the
  v0 corpus.
- **8.6 unordered SUP/DUP** [blocked]: awaiting upstream HVM4.
- **8.7 WL bridge** (a-d): `TATP[axioms, conjecture]` returning
  `Association[Status, Steps, Rules, QueueSize]`; encoder maps
  `Pattern[x, _]` -> FVR, `Symbol[s]` -> nullary CTR,
  `head[args...]` -> CTR with children.  Reachable from
  Wolfram notebooks for hand-written equational problems.
- **8.8 --mix CP-priority heuristic**: penalty
  `MIX_UNORIENTED_PENALTY = 4` for unorientable CPs.  Pop-
  order change only; soundness preserved.
- **8.9 narrowing for existential goals** (a-e):
  `thvm_atp_narrow_step` walks non-variable positions trying
  unification with rule LHSs; `goal_existential` flag in
  `goal_check` dispatches; `EXISTS` `.pr` syntax;
  `TATP[..., Witness -> {x_}]` WL surface returning bound
  witness terms.
- **8.10 SupGen-style search** (a, b; c is this memo):
  recognized that the existing CP-priority queue (5.3) is
  already SupGen-flavored; surveyed 4 candidate extensions and
  shipped `thvm_atp_peek_top_k` as a small demonstrative API.

## What stayed deferred

Three items remain `[blocked]` with documented forward-looking
reasons:

- **8.2d full pure-IC port of `thvm_kbo`**: deferred per
  `kbo_ic_design.md` until SupGen-style search creates a use
  case.  The PRI wrapper (8.2b) and pure-IC `kbo_eq` slice
  (8.2c) are sufficient for current needs.
- **8.6 unordered SUP/DUP**: awaits HVM4 upstream landing the
  feature.  The labeled-SUP approach in 8.1d-ii is sufficient
  for v0.

The blocks are forward-looking, not failure-driven.  Each
includes a clear precondition for unblocking.

## Empirical findings worth preserving

Several mid-arc analyses produced honest, sometimes surprising,
findings:

1. **BDP connectedness is dominated by 7.1**: the
   `connectedness_design.md` lemma showed any rule subset
   `R' ⊆ R` cannot find joins R can't.  All three connectedness
   variants (subsumption, source-rule-disjoint, BDP-below-c)
   are strictly weaker pruning than full-R joinability.
2. **Rule-subsumption is similarly dominated**: same argument
   applies; rule subsumption fires only when an existing rule
   rewrites lhs to rhs, which 7.1 also catches.
3. **KBO and LPO agree on canonical group/monoid axioms**:
   stage 8.5d empirically confirmed this; classical KB
   literature predicts agreement when precedence and weight
   functions are aligned.
4. **IC-routed paths produce byte-identical counters to C
   paths**: 8.1e-iii (cp-gen), 8.3e-iii (rewrite), 8.5d (KBO
   vs LPO), and 8.10b (peek vs pop) all confirm structural
   parity.  Wall-clock differences are within run-to-run noise
   except on the IC-rewrite TIMEOUT case (~2x, at the edge of
   the target).
5. **Twee proves cases we time out on**: stage 7.4d showed
   Twee proves all 4 fixtures in 24-30 ms each; our group-
   commutativity goal stays TIMEOUT at budget=256.  The
   bottleneck isn't unification or CP gen (those are byte-
   identical to Twee in spirit) -- it's our limited heuristic
   sophistication and lack of AC matching.

## Natural follow-on stages

The arc ends with several genuine engineering and research
opportunities clearly framed:

1. **TPTP-UEQ corpus expansion + Twee comparison**: extend
   `tests/data/atp/` with GRP/RNG/LCL/LAT division problems;
   re-run `tools/bench_twee.c`; measure where IC-native ATP
   stands relative to the published baselines.  Stage 8.5d's
   "KBO and LPO agree" finding only holds for our 5 v0
   fixtures; broader corpora are likely to discriminate.
2. **AC matching** (associativity-commutativity): Waldmeister
   and Twee both ship AC-aware joinability; major win on
   group-axiom problems.  Substantial new infrastructure
   (AC-canonicalization, AC unification).
3. **Full pure-IC `thvm_kbo`**: 8.2d's deferred research
   target; lets 8.10's deeper SupGen integration superpose
   KboConfigs themselves.
4. **Sort-aware KBO**: 8.4's design memo deferred this; lets
   sort-discriminating signatures get tighter orientations.
5. **AVATAR architecture / clause splitting**: Vampire's
   approach to decomposing large saturation problems.  Out of
   scope for unit-equational logic but the underlying ideas
   transfer.
6. **TPTP file parsing in WL**: stage 8.7's design memo flagged
   `wald_parse_file` is already a thin wrapper away from a WL
   surface (`TATP[File["foo.p"]]`).
7. **Multi-witness narrowing enumeration**: stage 8.9's design
   memo flagged this as v0 limitation -- only first witness
   returned.
8. **Heap-resetting mechanism**: 8.3e-iii's bench finding that
   IC-rewrite at budget=256 overflows HEAP_CAP suggests a
   bump-pointer reset between saturation steps would let IC
   modes default on.
9. **Trace-level SupGen**: the genuine SupGen vision -- a
   superposed search tree of saturation traces, with
   priority-aware collapse exploring multi-step look-ahead.
   Multi-firing research item; 8.10a documented why it stayed
   out of v0.

## Verification snapshot at arc close

- C suite: 58 test files (9-10 atp-related), ~5500+ sub-checks
  in `test_wald.c`, ~8400+ in `test_atp.c`.
- WL suite: 314 verification tests across `wl/THVMLink/Tests/`.
- Bench harness: `make test` regenerates `build/bench-atp.csv`
  with 20 rows (5 fixtures x 4 cp-gen / rewrite modes); `make
  bench-twee` regenerates `build/bench-twee.csv` with the Twee
  comparison.

Both suites green at every stage's commit; no regressions
introduced across the arc.

## What this arc demonstrated

The IC-native ATP arc accomplished its primary stated goal:
**a working IC-native equational theorem prover that can be
driven from Wolfram notebooks via `TATP[...]`**.  The arc also
validated several research claims with empirical findings that
were sometimes contrary to expectation (BDP connectedness
domination, KBO-LPO agreement on small axiom sets) and
contributed substantive infrastructure (`TAG_PRI`, APP-SUP,
multi-sort metadata, narrowing primitives) that's reusable
beyond the ATP context.

The arc closes here.  Further development happens via discrete
follow-on stages with their own design memos.

## File index

Design memos written across the arc:

- `docs/plans/waldmeister_ic_atp.md` (stage 1) -- foundational
  research memo
- `docs/plans/saturation_loop.md` (5.x) -- algorithm sketch
- `docs/plans/connectedness_design.md` (7.2a) -- BDP analysis
- `docs/plans/sup_encoded_cps.md` (8.1a)
- `docs/plans/kbo_ic_design.md` (8.2a)
- `docs/plans/icc_sort_dispatch.md` (8.3d) -- resolution memo
- `docs/plans/ic_rule_dispatch.md` (8.3a)
- `docs/plans/multi_sort.md` (8.4a)
- `docs/plans/lpo_design.md` (8.5a)
- `docs/plans/wl_atp_bridge.md` (8.7a)
- `docs/plans/narrowing_design.md` (8.9a)
- `docs/plans/supgen_search_design.md` (8.10a)
- `docs/plans/atp_arc_summary.md` -- this file

Bench reports: `docs/bench-atp.md`.
