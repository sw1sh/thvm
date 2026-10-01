# ENIGMA dataset + guidance experiment harness

Tooling for the ATP machine-learned guidance roadmap
([docs/plans/atp_ml_roadmap.md](../../docs/plans/atp_ml_roadmap.md)): build a
proof corpus, train the Tier-2 GNN (Graph Neural Network) proof-relevance
scorer, and measure GNN-guided critical-pair reranking.

The WL scripts below run through `wl -t SEC -f script` (the fast WL CLI, `-t`
self-kills its kernel on timeout so a runaway leaves no orphan), one job at a
time, so a diverging or memory-heavy proof cannot accumulate kernels and
pressure the host.

## Dataset sources

Three complementary sources feed the training corpus:

1. **AxiomaticTheory / TPTP graph datasets** -- the WL scripts below
   (`prove_theory.wls`, `tptp_build.sh`) emit labelled CP-graph `.wxf`
   files under `$ATP_DATA`.
2. **C-side labelled feature TSVs** -- `THVM_ATP_ENIGMA_EXPORT=<path.tsv>`
   on any engine run (`bin/FindProof` or the bench binaries) records the
   14 hand features of every processed critical pair and, when the run
   PROVES, labels them by proof-DAG reachability (positives = CPs on the
   proof) and appends one labelled row per CP to the TSV.  Verified
   2026-07-16: a LAT171-1 run appends ~50k rows under a
   `label + 14-feature` header.
3. **The CASC proof corpus** -- `~/.cache/thvm/casc/enigma_dataset/`
   holds the harvested competition sweeps (as of 2026-07-16): 824
   winning proofs (`casc29_proofs/` 276, `casc30_proofs/` 276,
   `cascJ12_proofs/` 272) plus per-problem CP-feature TSVs
   (`*_features/`, one per solved problem, tagged with the winning
   schedule slice) and the merged `ds_casc*.tgz` bundles.

## Environment

- `ATP_DATA` -- scratch dir for datasets + scorers (default `/tmp/enigma`).
  Layout: `ds/` (AxiomaticTheory `.wxf`), `tptp_ds/` (TPTP `.wxf`),
  `proved.wl` (theory -> proved-theorem-name map), `*.safetensors` (scorers).
- `TPTP_ROOT` -- TPTP install root (so `include('Axioms/..')` resolves).

## Scripts

| script | what |
|---|---|
| `prove_one_tptp.wls` | prove one TPTP `.p`, emit proof-essential lemmas as a labelled graph `.wxf` (positives) |
| `tptp_build.sh <list> [out]` | per-problem TPTP build over a list of `.p` paths, resumable |
| `prove_theory.wls` | build the graph dataset for one AxiomaticTheory's proved NotableTheorems (pos+neg) |
| `build_all_safe.sh` | build all fast AxiomaticTheory datasets, one theory per `wl -t` |
| `merge_train.wls <out>` | merge ds/ + tptp_ds/, balance, train a small GNN scorer |
| `verify_fix.wls` | train the bigger GNN on the FULL dataset (minibatched, memory-bounded; issue #8) |
| `loop_measure.wls <scorer> <period> <maxSteps> <out>` | A/B solved-within-budget, rerank on vs off |
| `loop_timed.wls <period> <maxSteps>` | timed A/B (wall + solved) on a fixed theorem set |
| `rss_probe.wls` | peak-RSS probe of the minibatched big-model train (issue #8) |
| `eval_heldout.wls` | held-out scoring of a trained scorer |
| `edge_typed_eval.wls` | edge-typed GNN variant evaluation |
| `train_tier1_logistic.py` | train the Tier-1 LINEAR CP scorer (numpy) from the C-exported feature TSVs; emits the run-time weights blob |

`period 1e9` in the measure scripts effectively disables reranking (baseline).

## Tier-1 learned scorer: run-time loader + opt-in lane

The 14-feature logistic scorer (`ATP_CP_WEIGHT_LEARNED`, thvm.h) is
wired as an opt-in selection lane, default OFF (byte-identical unset):

- `--enigma` / `THVM_ATP_ENIGMA=1` routes CP-queue priority through the
  learned scorer instead of the classical size/goal heuristics.
- `--enigma-weights=<path>` / `THVM_ATP_ENIGMA_WEIGHTS=<path>` loads a
  trained model from a whitespace-separated f64 text file (the
  `thvm_atp_set_learned_scorer` blob: kind, hidden, mean[14],
  inv_std[14], then LINEAR w[14],b or MLP W1/b1/w2/b2).  Unset keeps the
  baked-in `ATP_LEARNED_W` defaults.

End-to-end training loop:

```
# 1. Export labelled feature TSVs from proving runs (one per corpus).
for p in casc29/UEQ/*.p; do
  THVM_ATP_ENIGMA_EXPORT=train_c29.tsv \
    bin/FindProof "$p" --preset=waldmeister-lazy --wall=6
done
# 2. Train the LINEAR scorer (casc29+casc30 train, cascJ12 held out).
python3 tools/atp_enigma/train_tier1_logistic.py \
    train_c29.tsv,train_c30.tsv held_j12.tsv enigma_tier1_casc.weights 1 0.001
# 3. Deploy.
bin/FindProof problem.p --enigma --enigma-weights=enigma_tier1_casc.weights
```

`enigma_tier1_casc.weights` is the model trained this way (casc29+casc30,
held-out cascJ12 AUC ~0.92), committed as a reference artifact.

VERDICT (negative): the LINEAR Tier-1 scorer ranks proof-relevant CPs
well OFFLINE (held-out AUC ~0.92) but does NOT transfer to DRIVING
selection.  Deployed via `--enigma` it regresses easy provers (the `age`
recency feature induces a depth-first, heap-exploding order; LAT171-1
13.8s -> timeout, BOO034-1 0.0s -> heap-exhaust) and solves none of the
hard CASC UEQ targets (GRP690-1/692-1/678-1, RNG027-9, LCL306-10 all time
out at 60s under `--enigma --ord=lpo`, GRP690-1 also at 240s).  The
baked-in `ATP_LEARNED_W` (goal-connection + orientability dominant, tiny
age weight) does not explode but likewise cracks no target.  This is the
ENIGMA train/deploy distribution-shift gap: offline ranking on
baseline-recorded CPs is not online selection quality.  A viable learned
lane needs DAgger-style retraining on self-generated CPs and/or richer
(symbol-pattern) features than the 14 hand columns -- the 14 features are
essentially a re-weighted size/orientability heuristic, not the
symbol-level signal E's CSI_Enigma keys on.  Kept as opt-in infrastructure
(the prove -> label -> train -> reload loop now closes without a
recompile); NOT enabled by default and NOT added to SCHEDULE[].

## Tier-2 GNN scorer: symbol-independent graph convolutional network

The Tier-2 lane scores a critical pair from its anonymised term
*structure* (a graph convolutional network, GCN), so it transfers across
signatures the way the 14 hand features cannot. The in-engine forward
runs on thvm's own tensor runtime in C (`atp_gnn_forward`, `src/atp/_.c`);
it is loaded at run time from a `.safetensors` file and drives the
critical-pair queue via a periodic re-rank hook. See
[docs/plans/atp_tier2_gnn.md](../../docs/plans/atp_tier2_gnn.md).

Pipeline (CPU-only; no WL, no GPU):

```
# 1. Export labelled CP GRAPHS from proving runs (one JSONL line per
#    processed CP: {"y",<0|1>,"x":[[12 feats]..],"e":[[src,dst]..]}).
#    SERIAL, one problem per process, small wall.
TPTP=~/.cache/thvm/casc bash tools/atp_enigma/gen_tier2_data.sh
#    (or directly, per problem:)
THVM_ATP_ENIGMA_GRAPH_EXPORT=train.jsonl \
  bin/FindProof problem.p --preset=waldmeister-lazy --wall=6

# 2. Train the GCN (numpy, CPU).  casc29+casc30 train, cascJ12 held out.
python3 tools/atp_enigma/train_tier2_gnn.py train \
  ~/.cache/thvm/enigma_tier2/train_c29.jsonl,~/.cache/thvm/enigma_tier2/train_c30.jsonl \
  ~/.cache/thvm/enigma_tier2/held_j12.jsonl \
  gcn_round0.safetensors                       # [H R steps lr neg_cap seed]

# 3. Parity: the Python forward must match the C engine forward bit-close.
make -f tools/casc/Makefile bin/gnn_score_dump
python3 tools/atp_enigma/train_tier2_gnn.py parity \
  train.jsonl bin/gnn_score_dump 64            # asserts max|diff| < 1e-3

# 4. Deploy: load the trained GCN + arm the re-rank hook (coop with the
#    Waldmeister primary heap every 2nd pick).
THVM_ATP_GNN_ASSET=gcn_round0.safetensors \
  THVM_ATP_GNN_RERANK_PERIOD=200 THVM_ATP_GNN_COOP_RATIO=2 \
  bin/FindProof problem.p --preset=waldmeister-lazy --ord=lpo --wall=60
```

The GCN forward is R rounds of `relu(A.h.W1 + h.Ws + b)` (row-normalised
adjacency with self-loops), masked-mean pool, two-class readout; the score
is `logit_pos - logit_neg`. `train_tier2_gnn.py`'s numpy forward is
op-for-op identical to the C forward (`parity` pins it to ~1e-9), so a
model trained here scores identically in the engine.

Knobs: `THVM_ATP_GNN_ASSET=<path>` loads a GCN, `THVM_ATP_GNN_RERANK_PERIOD`
sets the re-rank cadence (selections between re-ranks; 0 = off, the
byte-identical default), `THVM_ATP_GNN_COOP_RATIO=N` routes the GNN to the
secondary coop dimension (every N-th pick) instead of overwriting the
primary heap, `THVM_ATP_GNN_TOPK=K` is the parental pre-filter (each
re-rank GNN-scores only the top K CPs by the cheap score -- the Tier-1
logit if loaded, else cp_pri -- so a re-rank costs O(K) forwards no matter
how large the queue; default 512, 0 = whole queue). Completeness holds
regardless: the re-rank only permutes the queue, and the FIFO/AutoMaxWeight
fairness still fires.

### Round-0 result (behavior cloning; 2026-07-21)

`gcn_tier2_casc_round0.safetensors` is the round-0 model: H = 32, R = 3,
trained (class-balanced, 3000 Adam steps) on the graph corpus generated by
the recipe above over the solved casc29 + casc30 problems, held out on
cascJ12.

- **Dataset** (waldmeister-lazy, wall 6, serial): train 433,610 processed
  CPs / 4,865 proof-relevant (1.12%) from 192 re-proved problems; held-out
  cascJ12 255,171 CPs / 2,545 positive (1.0%) from 105 problems.
- **Offline (held-out cascJ12)**: AUC 0.842, P@100 34x / P@500 37x /
  P@1000 30x lift over the 1% base. vs the Tier-1 logistic baseline
  (0.918 AUC / ~17x). So round-0 is BELOW Tier-1 on global ranking AUC but
  ~2x its top-k precision lift. AUC plateaus at ~0.84 with H = 64 too, so
  it is FEATURE-limited (6 anonymised structural columns), not
  capacity/under-trained -- the expected behavior-cloning ceiling. The
  richer symbol-pattern features + DAgger retraining are what the
  literature (and this plateau) say close the gap; that is phase 2.
- **Online sanity (rerank armed)**: with the coop rerank at period 2000,
  the easy provers Tier-1 REGRESSED all still prove -- LAT171-1 18.8s
  (baseline 13.3s), BOO034-1 0.04s, COL002-5 22.8s (baseline 2.4s): slower
  but no timeout, unlike Tier-1 which lost them outright. At an aggressive
  period (200) the whole-queue re-scoring OVERHEAD regresses large-queue
  problems (LAT171-1 -> wall) -- the documented latency wall; a cheap
  parental pre-filter is the phase-2 fix. The hard targets GRP690-1
  (--ord=lpo) and RNG027-9 still time out at 60s (round-0 cracks neither,
  as expected). Deploy recommendation: coop, period >= 2000.

### Round-1 result (phase 2a: richer features + pre-filter; 2026-07-21)

Phase 2a fixed the two round-0 blockers.  (a) The **parental pre-filter**
(`THVM_ATP_GNN_TOPK`, default 512) caps each re-rank at K GNN forwards, so
an aggressive period no longer walls on a large queue.  (b) The node-
feature schema grew 6 -> 12: the positional signal (depth, argument index)
and the goal-relevance signal SInE lacks (`is_goal_symbol` from the
conjecture cone, `goal_closeness` from the reused s2a sine-level table),
plus `occurs_lhs/rhs`.  `gcn_tier2_casc_round1.safetensors` is the round-1
model: H = 64, R = 3, 3000 Adam steps, same recipe, F = 12.

- **Dataset** (F=12 regen, waldmeister-lazy wall 6, serial): train 471,248
  processed CPs / 4,866 proof-relevant (1.03%); held-out cascJ12 282,936
  CPs / 2,522 positive (0.89%).
- **Offline (held-out cascJ12)**: the goal + positional features BROKE the
  0.84 ceiling.  AUC 0.842 (round-0) -> 0.897 (H=32) -> **0.904 (H=64)**,
  approaching Tier-1's 0.918 while keeping symbol-independence.  P@100/500/
  1000 = 36x / 36x / 34x (H=64) -- ~2x Tier-1's ~17x and far more balanced
  across k than round-0 (46x@100 falling to 24x@1000).  Round-0 plateaued
  at ~0.84 for BOTH H=32 and H=64 (feature-limited); now H=64 > H=32, so
  capacity helps again -- confirming the plateau was feature poverty.
- **Pre-filter (latency wall)**: on GRP690-1 (queue ~45k CPs, coop period
  200) topk 512 does 832 saturation steps / 27.5 steps-sec in a 30s wall vs
  the whole-queue 649 / 21.7 (+27% throughput).  LAT171-1 at period 200 +
  topk 512 proves in 0.2s -- no wall (round-0 walled at period 200).
- **Online (round-1 armed, coop, topk 512)**: easy provers all still prove
  -- LAT171-1 14.2s -> 0.2s (period 200) / 0.9s (period 2000), BOO034-1
  0.0s, COL002-5 2.5s -> 26s (slower, not lost).  Held-out ANA034-10 2.5s
  -> 2.7s.  Hard targets GRP690-1/692-1/RNG027-9/LCL306-10 still time out
  (60s and 240s) -- offline ranking gain does not yet convert to online
  cracking (the distribution-shift gap), which is what phase 2b's DAgger
  self-play retraining targets.  Soundness canary (satisfiable problems
  with the rerank armed) reports no refutation.  Deploy: coop, period 2000,
  topk 512.

### Phase 2b: DAgger self-play retraining (2026-07-21)

DAgger (Dataset Aggregation) closes the train/deploy shift by retraining
on the critical pairs the LEARNER-guided search actually visits.  Round k
-> k+1: roll out FindProof with round-k ARMED (coop, period 200, topk 512)
AND the graph export ON over the training corpora, aggregate the guided
graphs with all prior rounds, retrain.  Driver: `dagger_round.sh` (dedup
384-unique casc29+casc30, RESUMABLE, keeps every PROVED problem's rows,
logs per-problem kbo/npos), `dagger_eval.sh` (easy + hard online),
`dagger_missscan.sh` (the 35 CASC-30 UEQ misses).

Rollout mechanic: arming (`THVM_ATP_GNN_ASSET` + rerank) and export
(`THVM_ATP_ENIGMA_GRAPH_EXPORT`) are independent env-gated paths that share
the sine-level table, so they compose with no code change -- the export
captures exactly the guided processed-CP stream, labelled on PROVED by the
proof-DAG.

Labeller caveat (measured, not a regression): `thvm_atp_proof_extract` is
a single-normal-form extractor; when the goal closes only by the MNF
(multi-normal-form) search its two sides do not join under the final rule
set (`kbo_eq=0`) and it returns 0, so every proof-relevant CP of that
proof is mislabelled negative.  Arming changes the kept rule set, so it
raises this rate (e.g. BOO014-10 labels 18 -> 0 armed).  The guided
NEGATIVES are the DAgger signal, so every PROVED problem's rows are kept
(the false-negative fraction is ~= the 1% positive rate, small); the
`.stats` file records `kbo=0` per round for honest accounting.

Per-round stats (wall 6, serial, 384-unique corpus):

| round | armed solved | vs baseline 136 | graphs | positives | extract-fail |
|---|---|---|---|---|---|
| baseline (unarmed) | 136 | -- | -- | -- | -- |
| round-1 armed | 73 | LOST 67, GAINED 4 | 25466 | 1003 | 18 |
| round-2 armed | 75 | LOST 68, GAINED 7 | 28164 |  938 | 18 |

Held-out cascJ12 AUC / P@k (FIXED baseline-distribution held-out, so the
numbers are comparable across rounds):

| model | train data | AUC | P@100 | P@500 | P@1000 |
|---|---|---|---|---|---|
| round-1 | baseline | 0.904 | 36x | 36x | 34x |
| round-2 | baseline + roll-r1 | 0.902 | 50.5x | 41.7x | 37.0x |
| round-3 | baseline + roll-r1 + roll-r2 | 0.905 | 56.1x | 38.8x | 34.0x |

Online eval (round-2 / round-3, coop topk 512): easy provers kept
(LAT171-1 0.2s, BOO034-1 0.06s, COL002-5 9.3-9.8s); the hard targets
GRP690-1 / GRP692-1 (--ord=lpo) / RNG027-9 / LCL306-10 all still time out
at wall 60.  Miss-scan (round-3 armed, 35 CASC-30 UEQ misses, wall 60):
0 flips.

VERDICT (honest, negative-but-measured): DAgger monotonically SHARPENS the
offline ranker (P@100 36x -> 50.5x -> 56.1x, AUC flat ~0.90) but does NOT
convert to online solves.  Guidance stays net-negative vs the unarmed
baseline (armed 73-75 vs 136 solved at wall 6) and cracks no hard target
or miss.  Three bottlenecks, none of them model capacity (AUC plateaus at
~0.90 for H=64, so a deeper net is not the lever):

1. **Pre-filter starvation.** The topk=512 cap scores only the top 512 CPs
   by the cheap score; a hard problem's queue is ~45k CPs (GRP690-1), so a
   proof-relevant CP outside that window is never scored.  The GNN cannot
   find a needle the cheap prefilter already discarded.
2. **Failures are unlabelled.** The 67 problems guidance LOSES prove
   nothing, so they yield no rows -- DAgger never sees corrective data for
   the exact problems where guidance fails.  This is the structural limit
   of DAgger-for-ATP on hard targets.
3. **Rerank cost > ranking gain at short walls.** Coop steering plus the
   rerank overhead slows the solve enough to lose ~half the baseline set
   at wall 6; the offline ranking edge does not pay for its online cost.

Next levers: a per-CP cost/latency signal (the rerank must justify itself),
a cheaper full-queue scoring so huge-queue proof CPs are visible (drop the
topk cap), or a value/lookahead model rather than a pointwise CP ranker.
`gcn_tier2_casc_round2.safetensors` / `round3` are committed as the DAgger
artifacts; the lane stays opt-in and is NOT added to `SCHEDULE[]`.

## Selection-time top-K re-ranker (neural selection phase 0d)

At every weight pick the engine's K best queued CPs are scored by a small
listwise head (rank, queue size, priority, the 14 push features, age) and
the argmax is selected. Offline it beats the engine's pick on held-out
problems; online it solves the same set as the twee lane (see
[docs/plans/atp_neural_selection.md](../../docs/plans/atp_neural_selection.md)),
so it stays opt-in and out of `SCHEDULE[]`. Pod-only pipeline over a
history-export harvest (`np0_harvest.sh` with `THVM_ATP_HISTORY_CPFEAT=1`):

```
topk_net.py prep    harvest/npz pool.tsv prep precheck.txt 8   # queues, top K, inputs
topk_net.py sanity  prep pool.tsv
topk_net.py train   prep pool.tsv runs/R32h32 R32h32
topk_net.py valgate runs/R32h32 prep pool.tsv validation/R32h32_val
topk_net.py export  runs/R32h32 models/topk_rerank_R32h32.safetensors
# engine: FindProof <twee lane flags> --rerank-topk=32 \
#   --rerank-model=tools/atp_enigma/models/topk_rerank_R32h32.safetensors
# THVM_ATP_RERANK_DUMP=<tsv> dumps the engine's candidates, inputs and scores:
topk_net.py parity     runs/R32h32 models/topk_rerank_R32h32.safetensors dump.tsv out.txt
topk_net.py featparity export.npz dump.tsv out.txt   # same run, history export at rate 1
```

Phase 0e (DAgger): the same harvest with the R32h32 head ARMED gives the
states the re-ranker walks itself; one head is trained on both trajectories
to convergence. `models/topk_rerank_D32h64_dagger1.safetensors` (K 32,
hidden 64) was made by exactly these commands (the harvest on 8 vCPUs):

```
THVM_ATP_HISTORY_CPFEAT=1 np0_harvest.sh bin/FindProof <TPTP> pool.tsv twee-rerank \
  "<twee lane flags> --rerank-topk=32 --rerank-model=models/topk_rerank_R32h32.safetensors" \
  rr_harvest 60 8 1 3500 100000 400000
topk_net.py prep  harvest/npz pool.tsv prep_eng precheck_eng.txt 8 engine   # the phase-0c harvest
topk_net.py prep  rr_harvest/npz pool.tsv prep_rr precheck_rr.txt 8 \
  rerank:32:models/topk_rerank_R32h32.safetensors                            # + replay of the head
topk_net.py rrcheck prep_rr rr_harvest/err rrcheck.txt                     # vs the engine's counters
topk_net.py train2  d_runs/D32h64 D32h64 pool.tsv prep_eng prep_rr         # early stopping, patience 20
topk_net.py valgate d_runs/D32h64 prep_rr  pool.tsv d_val/D32h64_rr        # re-ranker-visited states
topk_net.py valgate d_runs/D32h64 prep_eng pool.tsv d_val/D32h64_eng       # engine-visited states
topk_net.py export  d_runs/D32h64 models/topk_rerank_D32h64_dagger1.safetensors
```

It beats R32h32 on both state distributions (held-out top-1 proof-CP hit
0.36 against R32h32's 0.24 on re-ranker-visited states) and shortens proofs
further online, yet solves twee's set at 60 s and one more problem of the 37
twee misses at 300 s; it stays opt-in as well.

## Lane matrix (docs/plans/enigma_portfolio.md section 5)

`matrix_tables.py` turns the harvested lane matrix
(`~/.cache/thvm/enigma/matrix`, described by its `FORMAT.txt`) into the M0
artifacts.  It only processes text, so it runs on the Mac.

```
matrix_tables.py --seal       # j13 rows -> ~/.cache/thvm/enigma/sealed (read-only); train + heldout -> matrix.tsv
matrix_tables.py --validate   # G0.1 -> g01.txt, rerun.tsv
matrix_tables.py --tables     # T1-T8 per split -> tables/ (summary.txt lists them)
matrix_tables.py --decisions  # section 5.3 -> decisions_5_3.txt, dev_t.tsv, train_t.tsv, supplement_jobs.tsv,
                              #   label_harvest_jobs.tsv, oracle_rerun_jobs.tsv
matrix_tables.py --all        # --validate, --tables, --decisions
```

The J13 rows stay sealed until the plan's final look.  After `--seal` every
mode reads `matrix.tsv` only and stops on a j13 row or on a path under the
sealed directory; the only j13 facts it prints are the sealed row count and
the archive sha256, and it checks that its outputs name no j13 problem.
Re-running `--seal` rebuilds both files from the sealed raw shards and checks
that they are byte-identical.  The job lists use the runner's shard format
(name, path, split, lane), so `runner.sh` takes them as they are.
`static_gates.py <tptp_root> <problem.p>...` prints the gate bits and live
lanes of the CLI's exit-fast lanes, the static replica that T7 checks.

## Synthetic -10 corpus (plan M2 deliverable 5, decision D1)

`FindProof --emit-ifeq10 problem.p` prints a CNF or FOF Horn problem in
TPTP's -10 unit-equality encoding ([CS18]) and exits.  It is a port of the
jukebox `horn-to-unit` pipeline that produced TPTP's -10 files (clausifier,
encoding and output naming, `tools/atp_cli/ifeq10.c`), written to
reproduce their dispatcher axioms, names, clause order and goal form.  None
of it has run yet: the checks below run on pods, and until they pass the
match with TPTP's files is a claim about the eleven reference cases, with
the ALL=1 survey as its held-out test.  The FOF front end (`fof_prepass.c`)
is not used: it encodes for proving, with one dispatcher and a
`false != true` goal; plan section 5.4 records this departure from the
wording of M2 deliverable 5.  Exit codes: 0 encoded, 2 input or write
error, 3 not Horn (the first non-Horn clause is named on stderr), 4 a
one-element model exists (an SZS Satisfiable line).  The header carries
the source's Status, mapped (Theorem becomes Unsatisfiable), with the SPC
that `np0_pool.py` reads, and an Encoding line,
`% Encoding : dispatchers D, tuples T, predicates P, definitions S`: the
ifeq dispatchers, the tuple symbols, the predicates turned into functions
into bool and the clausifier's sP<n> definition predicates.  D = T = P = 0
means that the source is unit equality already.

On a pod, in this order (the Mac only edits and syntax-checks).  Steps 2,
3, 5 and 6 are `tools/atp_enigma/ifeq10_pod.sh TPTP_ROOT INPUT_DIR OUT_DIR`
(step 6 with `CORPUS=1`), which also runs `tests/atp_ifeq10_diff.sh`, gives
every step a deadline, stops at the first failure and leaves sentinels;
steps 1 and 4 are the lane matrix's `pod_gates.sh`.  Copy to INPUT_DIR,
with a `SHA256SUMS` file the script checks:

| File | From the Mac | sha256 (2026-10-02) |
|------|--------------|---------------------|
| j13_ueq_names.txt | ~/.cache/thvm/casc/J13/neural_p0/ | c43f0c4914bb4b295040f2a3186631da7a8955f991a81e5b15939d7b9cd69410 |
| pool.tsv | ~/.cache/thvm/casc/J13/neural_p0/ | 9fdb0135c608d6ed49e54b77dc94d20810ea4494aeb7974a4d75965181130594 |
| dev_t.tsv | ~/.cache/thvm/enigma/matrix/ | c66cc386aa51e99f36960a79d1ba0667c38db8de078f39711701d9daebddae88 |

The TPTP-v9.3.1 tree is downloaded on the pod (as `pod_gates.sh` does);
the driver records its release and the sha256 of its problem list.

1. `make -f tools/casc/Makefile bin/FindProof` with `CC=gcc` and with
   `CC=clang-14` (const-object initializers broke clang 14, c59b353b), and
   the static musl build of `tools/casc/mk_bin_tarball.sh`.
2. `make bin/test_atp_ifeq10 && bin/test_atp_ifeq10`: the ten cases
   inlined from TPTP v9.2.1, the exits, the header and Encoding lines.
3. `TPTP=<TPTP root> ALL=1 tests/atp_ifeq10_tptp.sh bin/FindProof`: the
   eleven cases (the ten plus SYN143-1) and every other [CS18] pair of the
   tree outside the J13 stems, judged by the decision rule in the
   script's header.  A pass writes `bin/FindProof.ifeq10-pass`.
4. G2.3: ifeq10.c is part of FindProof's translation unit, so `bin/test_atp`
   in full and the OA and DN pins must not move (the commands are in the
   lane matrix's `pod_gates.sh`).
5. `tests/atp_ifeq10_corpus.sh bin/FindProof`: the driver on a fake
   TPTP-v9.3.1 tree.
6. Only then the corpus, on the TPTP-v9.3.1 tree:

```
P=~/.cache/thvm/casc/J13/neural_p0
awk -F'\t' '$5 == "heldout" { print $1 }' $P/pool.tsv > devh_names.txt
awk -F'\t' '$5 == "train" { print $1 }' $P/pool.tsv > pool_train_names.txt
TRAIN_NAMES=pool_train_names.txt tools/atp_enigma/make_ifeq10_corpus.sh \
  bin/FindProof <TPTP-v9.3.1 root> <out_dir> $P/j13_ueq_names.txt \
  devh_names.txt ~/.cache/thvm/enigma/matrix/dev_t.tsv
```

walks the tree's CNF and FOF problems with status Unsatisfiable or Theorem
and writes `TRAIN-X/` and `DEV-X/`, each a TPTP-style root (`Problems/DOM/`,
emitted name = source name + `-10`) with a `pool.tsv` in `np0_pool.py`'s
columns, split 80/20 by `matrix_tables.py`'s `stem_hash` of the stem.
Skipped, with the reason in `manifest.tsv`: every stem of a J13 name (which
drops the -1/-2/-3 sources of J13's -10 theorems), every stem of an
exclusion list (above: DEV-H and DEV-T, so that neither has an encoded copy
in the corpus), problems that are unit equality already (by their SPC, or
after clausification by the Encoding line: the FOF versions of UEQ
problems), stems that TPTP already ships as -10 problems (the pool holds
them), DEV-X problems whose stem occurs in the pool's train split, and
files whose header could not be read (`noheader`).  An encoding that
equals a J13 problem file modulo variable and symbol renaming, clause
order and names (`ifeq10_keys.py`) is dropped as `j13dup` whatever its
stem.  The exclusion lists and TRAIN_NAMES are required; `ALLOW_LEAKY=1`
runs without them and ends `status=complete-unguarded`, which M3 must not
train on.
DEV-X is a held-out set: its stems are disjoint from TRAIN-X, J13, DEV-H,
DEV-T and the pool's train split.  The manifest also carries each source's
rating (training weights, plan section 3.2), its includes (the axiom sets
behind the axiom-novel subset, section 1.4) and its number of sP
definitions (encodings that no TPTP -10 file is a reference for).

The driver fingerprints its inputs (the binary, the lists, the TPTP
release and problem list, itself, `ifeq10_keys.py` and `ifeq10_diff.py`,
TIMEOUT_S, MEM_MB, ALLOW_LEAKY) and resumes only on
the same inputs; it requires the pass file of step 3, refuses a tree other
than TPTP v9.3.1 (T-NEW counts from that release, and at L2 T-NEW must
also drop every stem that `manifest.tsv` lists as emitted), checks the
data rule on its own output (J13 duplicate keys included) before it
writes `.done`, and writes ALERT
lines for what can only be an encoder bug: sat1 on an unsatisfiable
source, nonhorn on an `_HRN` source, a truncated output.

The stem of a name is its DOMNNN prefix (`^[A-Z]{3}[0-9]{3}`).  A tool that
needs the stem or the split of a corpus problem reads `manifest.tsv` (stem,
split) or `pool.tsv`, or takes that prefix; it never recomputes either from
`name.split("-")[0]`, which is `PUZ131+1` for `PUZ131+1-10` and would
separate a FOF-derived problem from its CNF siblings.  `matrix_tables.py`'s
`stem()` is that split: it agrees with the prefix on every pool and J13 name
and is not for corpus names.  DEV-X's `pool.tsv` labels its rows
`heldout`, as `np0_harvest.sh` expects of an evaluation set: never
concatenate it with the pool's `pool.tsv`, whose `heldout` rows are DEV-H.

`ifeq10_diff.py OURS THEIRS` compares two CNF files modulo clause order and
variable renaming; on a match it says whether the (name, clause) pairs or
the clause order differ, on a miss whether they agree with generated
symbols renumbered or ifeq conditions taken as sets; it exits 2 on an
unreadable file or a term nested too deep to compare.
`ifeq10_keys.py TPTP_ROOT LIST` prints a key per problem file that is
blind to variable and symbol renaming, clause order and names (includes
resolved), the corpus's J13 duplicate check.

## Typical flow

```
export TPTP_ROOT=/path/to/TPTP-v9.2.1 ATP_DATA=/tmp/enigma
tools/atp_enigma/build_all_safe.sh                                  # AxiomaticTheory datasets
tools/atp_enigma/tptp_build.sh ueq_problems.txt                     # TPTP datasets
wl -t 450 -f tools/atp_enigma/verify_fix.wls                        # train the corpus scorer
wl -t 200 -f tools/atp_enigma/loop_timed.wls 200 800                # measure guided vs baseline
```

## Memory safety

`wl -t` bounds time but NOT memory; a diverging saturation can balloon the C
engine's CP queue past the system limit (it has no queue-size cap). So wrap
every proving/training pass in `safe_wl.sh RSS_CAP_MB -- <wl-cmd>`, which kills
the run's `WolframKernel` the moment its RSS exceeds the cap (poll 2s), well
below `tools/kernel_reaper.sh`'s system-wide backstop. `tptp_build.sh` already
does this per problem (4 GB). For a single train/measure run:

```
tools/atp_enigma/safe_wl.sh 6144 wl -t 450 -f tools/atp_enigma/verify_fix.wls
```

Prefer per-problem process isolation (one `wl` per problem, as `tptp_build.sh`
does) so memory never accumulates across problems in one kernel.
