# The ENIGMA selection layer (`src/atp/guide.c`)

The learned layer of a guided twin (docs/plans/enigma_portfolio.md,
section 3.1).  A twin is a base lane's flags plus `--guide=<family>`; at
every weight pick (the heap-root branch of `thvm_atp_select_cp`, once the
KD-heap, w2, goal, random and FIFO / age dimensions declined) a
compiled-in head chooses the CP to extract.  FIFO and age picks are
untouched.  The layer generalizes the top-K re-ranker `src/atp/topk.c`:
its 18-input heads compile in as layout `topk18`, with one divergence:
topk.c materializes a deferred (implicit) slot and scores its real push
features, while the layer reads them as missing (zeros), so a topk18
head picks differently from `--rerank-model` on raw-desc and deferred
lanes.

The layer does not pick under `THVM_ATP_KDHEAP` (the KD-heap branch takes
every pick; arming prints a warning) and refuses to arm beside the top-K
re-ranker (`THVM_ATP_RERANK_TOPK`), whose branch precedes it.

## Knobs

| Flag | Env | Meaning |
|---|---|---|
| `--guide=<family>` | `THVM_ATP_GUIDE` | arm the layer; the family picks the head's integration from its family table (`*` matches any family) |
| `--guide-zero` | `THVM_ATP_GUIDE_ZERO` | force the correction to 0: the twin selects exactly as its base lane, at the layer's full cost (arm PD, gate G2.2) |
| `--guide-model=<id>` | `THVM_ATP_GUIDE_MODEL` | the compiled-in head (default: the first in `src/atp/guide_models.c`) |

`thvm_atp_init` reads them once per process and arms every engine it
creates.  Development overrides (env only): `THVM_ATP_GUIDE_MODE=topk:<k>`
or `lazy:<k>[:<tau>]`, and `THVM_ATP_GUIDE_COOP=<r>`, replace the
family's integration, for choosing a family's mode on VAL.  Hosts and
tests arm with `thvm_atp_guide_arm(s, family, model_id, zero, lane)`
before the engine's first push.

**The dump.**  `THVM_ATP_GUIDE_DUMP=<path>` writes the scored candidates
of the dumped picks, the parity check's input and the training data's
(see "Training").  Each armed engine appends one header line,

    # guide model=... layout=... n_x=... hidden=... family=... mode=... k=... tau=...
      coop=... zero=... kappa=... lane=<index> lane_name=<lane> phi=<n set>
      every=<N> max=<picks> rows=<cap>

(one line), one row per scored candidate (t, the selection index; the
evaluation index; seq; chosen; x[0..n_x); the correction before
`--guide-zero`; the score) and, when the engine is freed, the footer
`# guide end picks=P rows=R`.  A session without its footer was cut short
(a kill), so its last line may be partial.  Which picks are dumped:

| Env | Default | Meaning |
|---|---|---|
| `THVM_ATP_GUIDE_DUMP_EVERY` | 1 | a pick at selection t is dumped when `atp_hx_hash(t) % N == 0`: about one selection in N over the whole run, every scored call at that t (orphan re-picks too) |
| `THVM_ATP_GUIDE_DUMP_MAX` | 400 | the dump stops after this many picks |
| `THVM_ATP_GUIDE_DUMP_ROWS` | 0 (none) | the dump stops before the first pick that would pass this many rows |

The sample is a hash of the selection index, not a stride: a stride would
alias with the age (FIFO) and coop cadences, whose picks the layer never
scores.  A pick's rows are written whole and flushed.  Writing is pure
observation, so it runs in a work-clock exclusion window in every twin,
and a zero twin with the dump on selects exactly as its base lane
(`test_atp_guide` zero-exact-dump, the pins in the pod battery, G2.2 with
`DUMP_EVERY`).  `weights_to_c.py featparity` rebuilds the per-problem
means from the dumped picks, so it needs an unsampled dump (N = 1).

**The lane.**  The lane one-hot is the lane's own: the head's lane table
(`LaneNames`) is searched for `THVM_ATP_GUIDE_LANE`, else
`FINDPROOF_SLICE` (the name every schedule slice and `--lane` run
carries), a twin `enigma-<base>` reading as `<base>`; a lane the table
does not name takes the family entry's index.  So every twin of a
family that spans several lanes (the twee lanes, say) sees the column
its harvest's lane tag gave it in training.

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
- **coop**: the r-th, 2r-th, ... non-trivial weight selection goes to the
  plain engine (the root, unscored).  The layer's own counter advances
  when the pick survives the orphan test, so an orphan re-pick repeats
  its selection's decision, and trivial picks (one CP queued, an
  ultimate root) do not count.  The w2_modulo branch is not used: it
  scans the whole heap.

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
| 17 | log1p(n_cps) | 19 | term-heap growth since arming, in bytes, over the RSS guard's cap (`THVM_ATP_RSS_ABORT_MB`, default 8 GB), floored to 1/64, at most 2 |
| 18 | log1p(pushes per selection over the last 64 selections) | 20.. | phi(p), then the one-hot of the family's lane id |

f4 (a copy of f0) is dropped.  `*` marks the columns normalized per
problem: `log1p(v) - m`, where m is the mean of `log1p(v)` over the first
candidate scored at each pick: exact over the first 256 such picks, then
an exponential mean with weight 1/256, updated before the pick's
candidates are scored.

Column 19 measures memory by the search's own term heap (FindProof runs
no collector, so it only grows), not by the process RSS: the RSS depends
on the allocator, page faults and the host, and counts the layer's own
memory, so a head reading it would not pick reproducibly.

In a lazy integration every candidate is the engine's next one when it
is scored, so x[0] = 0: a lazy family's head is trained on rows with
x[0] = 0 (the parity tool checks a lazy dump for it).

Layout `topk18` is `src/atp/topk.c`'s input: log1p(rank), log1p(n_cps),
log1p(cp_pri), f0..f13 raw, log1p(selections since push).

**phi(p) seam.** The layer reads problem features from
`thvm_atp_guide_set_phi(const AtpGuidePhi *)` (`n` values, at most 64),
set in the process that runs the engine before its first weight pick; it
folds them in at that pick and again whenever they are set anew.  A
non-finite value, or a column past `n`, reads as its training mean.

FindProof sets them in every lane process whose environment arms a head
that reads phi(p) (`thvm_atp_guide_env_phi_cols() > 0`): after the front
end, whose budgets it must not move, and before the engine's wall, alarm
and emission deadline are set, `cli_guide_phi`
(tools/atp_cli/problem_features.c) computes the row `--problem-features`
prints with the same function in the same forked child under the same
1 s CPU budget (`tp_problem_features`, `phi_compute`), copies its
`ATP_PHI_N` (40) columns into an `AtpGuidePhi` and calls
`thvm_atp_guide_set_phi`.  The plan's single function (section 3.1) is
thus the one row both sides see: `tests/atp_guide_phi.sh` runs lanes with
different front ends and checks that their dumps carry x[20..60) equal to
the `--problem-features` row bit for bit.  A problem without the row
(past the budget, not unit-equality CNF, a .pr file, a failed replay)
runs without the layer, as section 3.1 prescribes for a problem past the
budget: `thvm_atp_guide_env_off` prints `guide: <why>; selection layer
off` and no engine of the process arms.  A head that reads no phi(p)
(`example`, topk18) arms without the replay.  Unarmed, the seam is the
environment probe the engine's init would make anyway, so unarmed runs
are unchanged.

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
- In a `--guide-zero` twin the per-pick work runs in a work-clock
  exclusion window, so its clock-gated disciplines see the base lane's
  clock.  A deployed (non-zero) twin keeps its selection work on the
  clock: `--lrs` predicts its reach from the selection rate and the
  cp-set-ir budget meters the search, and both must see what a guided
  selection really costs, or LRS would prune too little.
- Outside the window an armed layer runs a few dozen instructions per
  step: the selection log, the two goal compares, the window's own edges,
  and the coop and trivial picks.  The twin's environment is also longer
  by its `THVM_ATP_GUIDE*` variables, and an uncached getenv in a hot
  path costs work in proportion to the environment's size.
  `--work-clock=instr` counts both, so in a clock-gated lane (`--lrs`,
  `--cp-set-ir-budget`) a decision can land a few selections apart from
  the base lane's; under the CPU clock such lanes differ from run to run
  anyway.  Lanes without a clock-gated discipline never read the clock
  and are exact.
- The RSS guard discounts the layer's own allocations (about 120 KB plus
  4 bytes per selection, the goal versions and the demotion bitmap), but
  not heap pages a pick's scratch decode touches past the base lane's
  high-water mark, so near the cap a twin's guard can abort a few polls
  apart from its base lane's.
- A non-finite correction (none can arise from validated weights and
  finite inputs) reads as 0 and is counted (`nonfinite=`).
- Per-pick cost counters print as one `% guide:` line on stderr after
  the engine stats: picks, scored, coop and trivial picks, changed picks,
  weight and coop selections, evaluations, feature decodes, missing
  features, heap nodes visited, demotions, accepted / fallback / blocked
  lazy picks, dirty picks, non-finite corrections, total time, the
  layer's time as a fraction of the time since arming (`frac_wall`, the
  plan's 5% cost gate), microseconds per scored pick and per pick, and
  the slowest pick.  Wall-stopped bench runs print it too (the bench
  stops at the wall and prints its stats); only the alarm backstop a
  second later does not.

## Gates G2.2 and G2.4

`tests/atp_guide_gates.sh` (pods only) runs, per family, lane and
problem, the base lane and its zero twin under the gate clock
`THVM_ATP_WORK_CLOCK=steps` (amendments A3 and A8), with `--lane=L
--proof --szs`, the selection trace and the clock trace, and prints one
PASS, FAIL or NONDET line each:

- The engine's clock-gated disciplines read the steps clock, which an
  observer cannot advance, so every row must agree exactly on all four:
  the selections pick by pick, the clock-gated decisions (clocks masked),
  stdout (status, proof) and the engine stats (LRS prune and recompute
  counters included), after masking timings, the goal path and the
  twin's `% guide:` line.  A clock-gated decision taken elsewhere is a
  FAIL: A8 withdrew A2's reading, under which a clock-gated lane could
  drift after its first differing decision.
- The twin arms through the variables `--guide`, `--guide-zero` and
  `--guide-model` set; the base carries `THVM_ATP_GUIDP*` variables of the
  same lengths in the same places, so both environments have one layout
  and cost the same to scan (the front-end budgets keep the instruction
  clock).
- The twin must print a `% guide:` line with picks > 0 and `frac_wall`:
  a layer that never picked (phi(p) missing for a head that reads it, the
  KD-heap or the top-K re-ranker armed) fails.
- `MODEL=<id>` picks the head (the loop-0 gates use `harvest`, so the
  twin also runs the phi(p) seam); `DUMP_EVERY=N` gives the twin the
  loop-0 harvest's sampled dump, which must not change the search either
  (the twin's dump must end with its footer); `AA=1` runs the base twice,
  and a row whose two base runs differ is NONDET: it cannot test the
  twin and does not count.
- Per family and lane (`OUT/g22.summary`), in list order: the lane passes
  when no row FAILs and `MIN_PASS` (8, the plan's count) rows PASS; fewer
  leave it INCONCLUSIVE.  `OUT/g22.tsv` has one row per problem.

With `HEAD=<head.safetensors>` (compiled in, `MODEL=<id>`) it also runs
the non-zero twin with the dump and `weights_to_c.py parity` (G2.4);
`FEATPARITY=1` adds the history export, `np0_extract.py` and
`weights_to_c.py featparity`.

## Open items

- Columns 13, 14 (goal symbols), 19 (term-heap growth), phi and the lane
  are not in history export v2, and a CP the run never selected has no
  content there, so its cross-lane key is unknown.  Training reads every
  input from the dump (label_join.py joins it with the export by seq);
  `featparity` checks the other columns against the export, and the
  phi(p) columns are checked against `--problem-features` by
  `tests/atp_guide_phi.sh` and by the join.
- A schedule with twins computes phi(p) once per twin process (each lane
  is its own exec); the plan's single parent-side computation would need
  the row handed to the twins without lengthening the unarmed lanes'
  environments.

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
    weights_to_c.py harvest hv.safetensors      # the loop-0 harvest head
    weights_to_c.py parity head.safetensors dump.tsv [out.txt]

A head is a safetensors file of f32 `mu`, `sd`, `w1` [hid, n_x], `b1`,
`w2`, `b2`, `kappa`; its metadata gives `Layout` (`v1` or `topk18`),
`Phi`, `Lanes` and `Families` (`name:lane:mode:k:tau:coop`, comma
separated).  A `topk_net.py export` file (no `Layout`) reads as `topk18`
with the one family `*:0:topk:K:0:0`.  Heads total at most 2 MB.  Two
heads ship (`build example=ex.safetensors harvest=hv.safetensors`
reproduces `src/atp/guide_models.c` byte for byte):
- `example` (v1, 22 inputs, no phi(p), hidden 4, family `*` top-K 16), the
  default, exists so the build compiles;
- `harvest` (v1, 60 inputs: every phi(p) column and no lane column, hidden
  1, w1 = w2 = 0, kappa 1, family `*` top-K 64) is the loop-0 harvest
  head: its correction is 0 with or without `--guide-zero`, so it picks
  the engine's root, and its dump records the engine's 64 best
  candidates, phi(p) included; a dump names its lane (`lane_name=`), from
  which the trainer builds the lane one-hot.

Metadata `LaneNames` (optional) lists the `Lanes` lane names in one-hot
order; `weights_to_c.py lanes head.safetensors listed.txt` checks them
against `FindProof --list-lanes` (every name a lane, in that order).
Model ids, family and lane names match `[A-Za-z0-9_.-]{1,47}` (a family
may be `*`), so a name is a safe C string literal.

`parity` rescores a `THVM_ATP_GUIDE_DUMP` file in float64, each session
(armed engine) apart: each candidate's correction and score to 1e-9,
each pick's decision (gate G2.4), and rank input 0 on every lazy row.
`featparity run.npz dump.tsv` rebuilds the first session's columns 0-12
and 15-18, and each top-K pick's candidate set, from the same run's
history export (`np0_extract.py` of a `THVM_ATP_HISTORY_RATE=1
THVM_ATP_HISTORY_CPFEAT=1` run, no negative subsampling) to 1e-9: the
per-problem running means, the selections since push and the queue
growth included.

## Training (M3)

The pipeline from a guided harvest to a compiled-in head, every step on a
pod (plan section 3.2):

1. `tools/atp_enigma/guide_jobs.py` lists the (problem, lane) jobs: every
   TRAIN and TRAIN-X pair of the families whose 60 s lane-matrix row shows
   the lane searched the problem (proved, wall, saturated, step cap,
   memory guard), failed runs included, each with the dump rate that
   spreads about 1,000 picks over a run as long as its matrix run.
2. `tools/atp_enigma/guide_harvest.sh` runs them: the label harvest's
   `FindProof --lane=L --szs --proof --wall=60` with export v2 at rate 1
   (and `--work-clock=instr` on the clock-gated lanes), armed as the zero
   twin of the harvest head with the sampled dump; `np0_extract.py` makes
   the npz (proof mode, hindsight mode for a failed run or a withheld
   proof), the dump is gzipped, and `phi.tsv` holds the problems'
   `--problem-features` rows.
3. `tools/atp_enigma/label_join.py keys` reads the proof keys and input
   signatures of every label-harvest and harvest npz; `join` turns each
   run's dump and npz into listwise groups (one per dumped pick:
   candidates, inputs, labels) with three label sources: the run's own
   proof (by seq), cross-lane positives (a selected candidate whose
   rule's canonical key is in another lane's proof of the problem, the
   input signatures equal) and hindsight positives (failed runs), and
   writes per-lane statistics of what each source adds.  A run whose
   dump disagrees with its export (the chosen CP at a dumped selection is
   not the one the export records) or with its phi(p) row is left out.
4. `tools/atp_enigma/guide_net.py train` fits a head on py/thvm: the
   listwise loss within each pick's candidate set (a segment log-softmax
   by scatter_reduce), layout v1 inputs with the lane one-hot, run
   weights by TPTP rating and run length, five stem-hash OOF folds over
   TRAIN and TRAIN-X together and the full model, early stopping on VAL,
   and exports the head (b2 = 0, never trained) with its `Layout`, `Phi`,
   `Lanes`, `LaneNames` and `Families` metadata.  Its readout (top-1
   proof-CP hit against the engine's pick; the median-percentile ratio on
   runs with 1,000+ selections; per lane, OOF and VAL) is a readout, not
   a gate: G3.1 is decided at loop 1 on armed-harvest states.
5. `weights_to_c.py build ... loop1=head.safetensors` compiles it in;
   `weights_to_c.py lanes` checks its lanes against `--list-lanes`, and
   G2.4 (`HEAD=`, `FEATPARITY=1`) checks C against Python.

Tests: `tests/test_atp_guide.c`, built with `THVM_ATP_GUIDE_CHECK` (a
clean pick that writes a cell-keyed KBO or LPO memo aborts) and
`THVM_ATP_TEST_CLOCK` (the test sets the wall and CPU clocks), and again
as `test_atp_guide_ac` with FindProof's AC and MNF defines: zero-correction
exactness on synthetic queues and on whole KBO, LPO, RPO, AC (in the AC
build), completion, goal-after-axioms and CP-set-interreduction runs,
selections, counters, trace length and extracted proof alike, with the
dirty fallback asserted under RPO and AC; whole `--lrs` runs pruned under
the test clock (zero twins prune as the engine; a non-zero lazy head's
demotions and the heap's keys survive each prune); the engineering traps
(no `cp_uid_a/b` without orphan murder; the trace cap does not steer a
non-zero pick); the lane one-hot; non-finite phi and corrections; forward
parity against a plain reference; the input columns; phi set after
arming; topk18 inputs against `thvm_atp_cp_features`; push features
against the history export's `F` records; the goal versions; a decoding
pick leaving the heap pointer, the memo epochs and the LPO statistics
unchanged; per-problem normalization; layered-lazy semantics; coop
cadence, orphan re-picks included; LRS side-key isolation; cost counters;
the dump, its sampler and row cap, and a zero twin's whole runs with the
sampled dump on; the harvest head; the phi(p) seam's environment probe
and switch-off; arming from the environment.  `tests/atp_guide_phi.sh`
checks the seam on FindProof itself, `label_join.py selftest` and
`guide_net.py selftest` the training tools on synthetic files (pods
only, like every test).
