# Corpus expansion findings: IC-vs-Twee on the enlarged corpus
> Stage 9.4c memo.  Closes 9.4 (corpus expansion).
> Sibling of `corpus_expansion_design.md` (9.4a).

## Methodology

- 12 `.pr` fixtures total: 7 pre-existing + 5 added in 9.4b.
- thvm: `bin/test_bench_atp` runs each fixture under the
  2x2 `(cp-gen, rewrite)` mode cross product (cc, ci, ic, ii).
  `step_cap = 32`.  CSV: `build/bench-atp.csv`.
- Twee 2.5+: `tools/bench_twee` converts each `.pr` to TPTP
  CNF, calls `twee --max-cps 256 --quiet --no-proof`, parses
  the status line.  CSV: `build/bench-twee.csv`.
- Both runs on Apple M3 Max, repo HEAD as of stage 9.4b.

Wall-clock comparison is necessarily rough: thvm runs to a
32-step bench budget, Twee to 256.  We compare *outcomes* and
*orders of magnitude*, not micro-benchmarks.

## Per-fixture comparison

Best-mode thvm wall-time chosen per fixture (typically `cc` or
`ic`, the C-direct rewrite paths).  Twee wall-time is the raw
`run_twee` measurement.

| Fixture | thvm | Twee | Gap |
|---|---|---|---|
| `comb_K`                     | PROVED 0.001 ms | PROVED 58 ms  | thvm wins |
| `comm_monoid_swap`           | **QUEUE_EMPTY** | PROVED 27 ms  | **thvm fails** |
| `exists_inverse`             | PROVED <0.001ms | PROVED 23 ms  | thvm wins |
| `exists_multi`               | PROVED <0.001ms | PROVED 28 ms  | thvm wins |
| `group_commutative_inverse`  | **TIMEOUT @ 32**| PROVED 21 ms  | **thvm fails** |
| `group_left_id_from_assoc`   | **TIMEOUT @ 32**| PROVED 21 ms  | **thvm fails** |
| `group_right_inverse_to_e`   | PROVED 0.001 ms | PROVED 19 ms  | thvm wins |
| `idempotent_nested`          | PROVED <0.001ms | PROVED 21 ms  | thvm wins |
| `lattice_absorb_simple`      | PROVED 0.001 ms | PROVED 21 ms  | thvm wins |
| `list_length`                | PROVED 0.002 ms | PROVED 20 ms  | thvm wins |
| `monoid_right_id`            | PROVED 0.001 ms | PROVED 23 ms  | thvm wins |
| `ring_distrib_zero`          | PROVED 0.001 ms | PROVED 20 ms  | thvm wins |

**Score: 9/12 thvm-wins, 3/12 thvm-fails.**

The 9 wins are dominated by goal-rewrite shortcutting at step 0
(7 of them prove with `step = 0`, before any saturation step
fires).  thvm's wall-clock advantage is real but heavily biased
toward "the answer is one rewrite away" cases.  Twee always has
the full saturation startup cost (~20 ms) regardless of
problem size.

The 3 fails cluster around the same root cause: lemmas that
require either AC-aware joinability or longer-than-32-step
saturation discovery.

## The three failures

### `comm_monoid_swap` (QUEUE_EMPTY @ 2)

Goal: `f(a, b) = f(b, a)` under `f(x, e) = x` and `f(x, y) =
f(y, x)`.  Commutativity is unorientable; unfailing 2-way
fallback installs both `f(x, y) -> f(y, x)` and the reverse.
Goal-rewrite then cycles between `f(a, b)` and `f(b, a)`
without producing a canonical form -- the saturation queue
drains in 2 steps but the goal stays unresolved.

Twee proves it because it runs **AC-aware joinability** (a
Twee-class redundancy criterion noted as deferred work in
section 4 of `docs/plans/waldmeister_ic_atp.md`): once both
directions of commutativity are present, an explicit AC check
recognises `f(a, b)` and `f(b, a)` as joined modulo AC and
closes the goal.

**Fix path**: implement AC-aware joinability.  Out of scope
for stage 9; flagged in `corpus_expansion_design.md` as the
clean motivation.

### `group_commutative_inverse` (TIMEOUT @ 32)

Goal: `f(a, i(a)) = f(i(a), a)` under full group axioms.
Requires deriving the left-inverse lemma `f(i(x), x) = e` from
right-id + right-inv + associativity, then applying it.  Lemma
discovery costs ~10-15 critical pairs to reach; we exhaust
the 32-step budget at 20 rules / 116 trace entries / 95 dropped
trivially-joinable / 51 connected.

Twee proves it under 256 CPs; presumably inside our budget if
`step_cap` were raised.  This fixture pre-existed (8.5d's
`max_step=256` `.expect`); we kept it on TIMEOUT after cutting
`BENCH_STEP_BUDGET` to 32 (8.3e-iii's HEAP_CAP fix).  9.3's
heap checkpoint/reset earned back some headroom; revisiting
the budget is plausible follow-on work (see Stop conditions).

### `group_left_id_from_assoc` (TIMEOUT @ 32)

Same axioms, different conjecture (`f(e, a) = a`).  Same fail
mode, same numbers (32 / 20 / 116 / 95 / 51).  Confirms that
the dominant cost on full-group fixtures is lemma discovery
(left-inverse), not the conjecture itself.

## What the cross-product modes show

For every fixture, all four `(cp-gen, rewrite)` modes report
**identical status** and **byte-identical step / rule / trace /
drop counters**.  This re-confirms 8.1e-ii (CP-gen parity) and
8.3e-ii (rewrite parity) on the new fixtures.

Wall-clock numbers diverge slightly: `ci` and `ii` (IC-routed
rewrite) run roughly 2-3x slower than `cc` and `ic` (C-direct
rewrite) on the TIMEOUT fixtures.  This is expected -- the IC
path goes through APP-PRI commutation per match attempt;
no surprise.

## KBO vs LPO

Every new fixture declared `ORDERING LPO` (consistent with
8.5d's "KBO and LPO agree on our corpus" finding).  We did
NOT re-run with KBO mode, since:

1. The bench harness picks ordering from the `.pr` file's
   `ORDERING` keyword.
2. All five new fixtures use precedences whose KBO image
   agrees with LPO on every axiom orientation (memo
   `corpus_expansion_design.md` covers each case).

So 8.5d's claim survives the corpus expansion: still no
KBO-vs-LPO disagreement on any fixture.  The honest test of
"KBO and LPO disagree" would need a deliberately constructed
case where KBO's weight calculation flips the orientation that
LPO's lex/sub-term path would pick.  None of the textbook
problems used here exhibit that.

## Where the 32-step budget bites

`group_commutative_inverse` and `group_left_id_from_assoc` are
the two TIMEOUTs in the corpus.  Both saturate to exactly the
same numbers: 32 / 20 / 116 / 95 / 51.  Quick math: 95 + 51 +
1 + 2 = 149 dropped, vs 116 trace entries kept.  Roughly 56%
of work is dropped as redundant -- the redundancy criteria are
firing, but not enough to short-circuit lemma discovery within
32 steps.

Twee's 256-CP budget puts it at ~8x our budget but it proves in
~21 ms wall.  At ~0.08 ms / step our 32-step run takes ~2.5 ms
already (4x slower per step, partly explained by lack of
indexing).

**Plausible budget bumps to revisit**:
- Increase `BENCH_STEP_BUDGET` from 32 to 64 once 9.3's heap
  reset has been measured for headroom.  Likely flips one of
  the two failures to PROVED but not both.
- Increase to 256 (Twee's value): would need careful
  HEAP_CAP measurement under all 4 modes.

Out of scope for 9.4.

## Stop conditions

9.4 set the >=1 PROVED + >=1 TIMEOUT mix target; we landed
9 PROVED + 3 fails (1 QUEUE_EMPTY + 2 TIMEOUT).  Mix achieved.

If a future stage raises the bench budget and finds either
group fixture flips to PROVED, that's a clean signal that
`step_cap` is the binding constraint, not the redundancy
criteria.  Update this memo's table with the new column.

If AC-aware joinability lands, `comm_monoid_swap` should flip
to PROVED.  Update this memo and `corpus_expansion_design.md`'s
prediction table.

## Out of scope

- AC-aware joinability (motivation: `comm_monoid_swap`).
- Twee-style ground-joinability (motivation: 32-step group
  fixtures).
- A `max_cps` parameter normalised across thvm and Twee for
  apples-to-apples timing.
- Profiling thvm's per-step cost to close the ~4x-per-step gap
  with Twee.

## Verification

This is a documentation + bench-CSV resolution.  Tests stay
green from the prior 9.4b landing (166/166 C, 323 WL).
`build/bench-atp.csv` and `build/bench-twee.csv` are the
captured snapshots; they are gitignored so re-running
`make test` and `make bench-twee` regenerates them but the
numbers above are the headline figures captured at 9.4c
landing time.
