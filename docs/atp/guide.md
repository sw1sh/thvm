# The ENIGMA selection layer (`src/atp/guide.c`)

The learned layer of a guided twin (docs/plans/enigma_portfolio.md,
section 3.1).  A twin is a base lane's flags plus `--guide=<family>`; at
every weight pick (the heap-root branch of `thvm_atp_select_cp`, once the
KD-heap, w2, goal, random and FIFO / age dimensions declined) a
compiled-in head chooses the CP to extract.  FIFO and age picks are
untouched.  The layer generalizes the top-K re-ranker `src/atp/topk.c`:
its 18-input heads compile in as layout `topk18`.

## Knobs

| Flag | Env | Meaning |
|---|---|---|
| `--guide=<family>` | `THVM_ATP_GUIDE` | arm the layer; the family picks the head's integration from its family table (`*` matches any family) |
| `--guide-zero` | `THVM_ATP_GUIDE_ZERO` | force the correction to 0: the twin selects exactly as its base lane, at the layer's full cost (arm PD, gate G2.2) |
| `--guide-model=<id>` | `THVM_ATP_GUIDE_MODEL` | the compiled-in head (default: the first in `src/atp/guide_models.c`) |

`thvm_atp_init` reads them once per process and arms every engine it
creates.  Development overrides (env only): `THVM_ATP_GUIDE_MODE=topk:<k>`
or `lazy:<k>[:<tau>]`, and `THVM_ATP_GUIDE_COOP=<r>`, replace the
family's integration, for choosing a family's mode on VAL.
`THVM_ATP_GUIDE_DUMP=<path>` (with `THVM_ATP_GUIDE_DUMP_MAX`, default 400
picks) writes every scored candidate for the parity check.  Hosts and
tests arm with `thvm_atp_guide_arm(s, family, model_id, zero)` before the
engine's first push.

## The score and the integrations

    s = -kappa log1p(rank) + correction
    correction = w2 . relu(W1 z + b1) + b2,    z = (x - mu) / sd

One hidden layer of at most 128 units; kappa >= 0.

- **top-K** (k <= 64): the engine's k best queued CPs, found by a
  best-first walk of the binary heap, are scored; the argmax is extracted
  (ties: the lower engine rank).
- **layered-lazy** (k <= 64 evaluations, 256 heap nodes per pick): the
  engine's next candidate is scored and accepted when `s >= -tau`
  (tau >= 0); otherwise it is demoted and the next is scored.  None
  accepted: the best scored one.  None reachable: the root.  Every
  candidate is the engine's next one when it is scored, so its rank input
  is 0.
- **coop**: every r-th weight pick goes to the plain engine (the root,
  unscored), counted by the layer's own counter.  The w2_modulo branch is
  not used: it scans the whole heap.

Demotion is a side key: a bitmap by seq.  A CP keeps its seq while it is
queued and no seq is stamped twice, so a set bit names one CP across
every row move, sweep and in-place repack.  `cp_pri`, `cp_seq` and the
heap order never change, so an `--lrs` lane prunes on exactly the keys of
its unguided run.  A demoted CP is skipped by later lazy walks while it
stays queued; FIFO, coop, fallback and blocked picks still reach it.

With `--guide-zero` the forward pass still runs; the correction is then
0, so top-K takes rank 0 and lazy accepts its first candidate.

## Inputs (layout v1)

`x` has 17 candidate columns, 3 state columns, `Phi` problem features
and `Lanes` lane columns.  f0..f13 are `thvm_atp_cp_features` at
age = seq against the goal normal form at the CP's push (the history
export's `F` record).

| col | input | col | input |
|---|---|---|---|
| 0 | log1p(rank) | 9 | f11, shares a goal symbol |
| 1 | log1p(cp_pri) * | 10 | f12, orientable |
| 2 | log1p(f0, size) * | 11 | log1p(f13, unification measure) * |
| 3 | log1p(f1, depth) * | 12 | log1p(selections since push), replacing f8 (the seq) |
| 4 | log1p(f2, variables) * | 13 | goal-symbol share of its CTR nodes, replacing f9 / f10 (symbol ids) |
| 5 | log1p(f3, variable occurrences) * | 14 | sides whose top symbol is a goal symbol, / 2 |
| 6 | log1p(f5, GT weight) * | 15 | log1p(cp_depth) * |
| 7 | log1p(f6, MIX2 weight) * | 16 | 1 when the push features are missing (a deferred slot; 2-11, 13, 14 are then 0) |
| 8 | log1p(f7, goal weight) * | | |
| 17 | log1p(n_cps) | 19 | RSS growth / cap at the RSS guard's last poll, floored to 1/64, at most 2 |
| 18 | log1p(pushes per selection over the last 64 selections) | 20.. | phi(p), then the one-hot of the family's lane id |

f4 (a copy of f0) is dropped.  `*` marks the columns normalized per
problem: `log1p(v) - m`, where m is the mean of `log1p(v)` over the first
candidate scored at each pick: exact over the first 256 such picks, then
an exponential mean with weight 1/256, updated before the pick's
candidates are scored.

Layout `topk18` is `src/atp/topk.c`'s input: log1p(rank), log1p(n_cps),
log1p(cp_pri), f0..f13 raw, log1p(selections since push).

**phi(p) seam.** The layer reads problem features from
`thvm_atp_guide_set_phi(const AtpGuidePhi *)` (`n` values, at most 64),
set in the process that runs the engine before its first weight pick; it
folds them in at that pick and again whenever they are set anew.
FindProof does not call it yet, so each phi column reads as its training
mean (z = 0).  The seam for the plan's phi(p) function (section 3.1: one
function serves `--problem-features` and deployment) is this call, with
the same row, in the lane's process before `atp_bench_main` runs.

## Hooks

Push-time state is reproduced from logs the layer keeps itself, so no
push and no row move reaches it:

- goal versions: the goal (normal form) the CPs pushed from a seq on are
  scored against, logged where the goal can change -- `thvm_atp_set_goals`,
  both goal checks of a step, the WM intake (one version from seq 0) --
  at `cp_seq_next`.  A version no CP was pushed under is replaced.
  `tests/test_atp_guide.c` checks the resulting push features against the
  history export's `F` records seq by seq.
- the selection log: `cp_seq_next` at every non-orphan selection, so the
  push step of seq q is the number of entries at or below q.
- the demotion bitmap and a feature cache by seq (cleared when the CP-set
  interreduction repacks the queue in place).

Unarmed, `s->guide` is NULL and every hook is one predictable branch.

## Exactness and cost

- FindProof runs no collector, so the candidates' pairs are decoded into
  heap scratch popped after the pick.  In plain-KBO, plain-LPO and
  combinator lanes the orientation columns come from `thvm_kbo_nomemo`,
  `thvm_lpo_quiet` and `atp_col3`, which touch no memo and no statistic,
  so the pop retires no memo.  The engine's persistent LPO memo is keyed
  on cell addresses and survives the engine's own heap resets, so a
  layer that invalidated it (as topk.c does) would change an LPO base
  lane's later comparisons.  Under WPO, RPO and AC (and an LPO side past
  the flat encoding) the layer falls back to topk.c's reset and counts
  the pick as dirty.  The memos are caches, so a zero-correction twin
  still makes its base lane's selections there, but the engine's later
  work differs.
- A deferred (implicit) slot is not decoded: its content is the raw
  overlap from the trace, and past the default trace cap that exists
  only with `--proof`.  The layer never reads `cp_uid_a/b`.
- The per-pick work runs in a work-clock exclusion window.  Outside it an
  armed layer runs a few dozen instructions per step: the selection log,
  the two goal compares, the window's own edges, and the coop and trivial
  picks.  `--work-clock=instr` still counts them, so in a clock-gated
  lane (`--lrs`, `--cp-set-ir-budget`) a decision can land one selection
  apart from the base lane's; under the CPU clock such lanes differ from
  run to run anyway.  Lanes without a clock-gated discipline never read
  the clock and are exact.  Gate G2.2 runs should keep the clock trace
  (`THVM_ATP_CLOCK_TRACE`) to tell a drifted decision from a layer bug.
- Per-pick cost counters print as one `% guide:` line on stderr after
  the engine stats: picks, scored, coop and trivial picks, changed picks,
  evaluations, feature decodes, missing features, heap nodes visited,
  demotions, accepted / fallback / blocked lazy picks, dirty picks, total
  time, microseconds per scored pick and per pick, and the slowest pick.

## Heads and the generator

Heads are compiled in: `src/atp/guide_models.c` is generated by
`tools/atp_enigma/weights_to_c.py` (static const float arrays with
hex-float literals, a family table per head, and the registry
`atp_guide_models()`).  A repo-relative model path would break under
StarExec's working directory, and a const object's value is not a
constant expression for clang 14, so every initializer is a literal or
an array's address.

    weights_to_c.py build ID=head.safetensors [ID=...] -o src/atp/guide_models.c
    weights_to_c.py example ex.safetensors      # the shipped tiny head
    weights_to_c.py parity head.safetensors dump.tsv [out.txt]

A head is a safetensors file of f32 `mu`, `sd`, `w1` [hid, n_x], `b1`,
`w2`, `b2`, `kappa`; its metadata gives `Layout` (`v1` or `topk18`),
`Phi`, `Lanes` and `Families` (`name:lane:mode:k:tau:coop`, comma
separated).  A `topk_net.py export` file (no `Layout`) reads as `topk18`
with the one family `*:0:topk:K:0:0`.  Heads total at most 2 MB.  The
shipped head `example` (v1, 22 inputs, hidden 4, family `*` top-K 16)
exists so the build compiles.

`parity` rescores a `THVM_ATP_GUIDE_DUMP` file in float64: each
candidate's correction and score to 1e-9, and each pick's decision
(gate G2.4).

Tests: `tests/test_atp_guide.c` (zero-correction exactness on synthetic
queues and on whole KBO, LPO, completion, goal-after-axioms and
CP-set-interreduction runs; forward parity against a plain reference;
the input columns; phi set after arming; topk18 inputs against
`thvm_atp_cp_features`; push features against the history export's `F`
records; the goal versions; a decoding pick leaving the heap pointer,
the memo epochs and the LPO statistics unchanged; per-problem
normalization; layered-lazy semantics; coop cadence; LRS side-key
isolation; cost counters; the dump; arming from the environment).
