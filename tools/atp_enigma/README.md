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
THVM_ATP_HISTORY_CPFEAT=1 CLOCK=cpu \
FLAGS="<twee lane flags> --rerank-topk=32 --rerank-model=models/topk_rerank_R32h32.safetensors" \
  np0_harvest.sh bin/FindProof <TPTP> pool.tsv twee-rerank rr_harvest 60 8 1 3500 100000 400000
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
matrix_tables.py --supplement # supplement/sup_K.tsv (180 s runner rows) -> supplement.tsv, supplement/validate.txt
matrix_tables.py --tables     # T1-T8 per split -> tables/ (summary.txt lists them); T1-T4 of the 180 s view too
matrix_tables.py --decisions  # section 5.3 -> decisions_5_3.txt, dev_t.tsv, train_t.tsv, supplement_jobs.tsv,
                              #   label_harvest_jobs.tsv, oracle_rerun_jobs.tsv
matrix_tables.py --all        # --validate, --supplement (when its rows exist), --tables, --decisions
matrix_tables.py --xjobs      # the synthetic -10 corpus's matrix (amendment A22): job lists in ~/.cache/thvm/enigma/ifeq10/matrix
matrix_tables.py --xmatrix    # its rows -> validate.txt, xmatrix.tsv, tables/ (T1-T4, T8, X10 per split), label_harvest_jobs.tsv
matrix_tables.py --xtwins     # section 5.3's twin rule over TRAIN and TRAIN-X (amendment A24) -> decisions_a24.txt
```

The X modes run the same matrix on the corpus that passed G2.6 (section
"Synthetic -10 corpus" below), in their own directory.  `--xjobs` deals
the 1,860 TRAIN-X and DEV-X problems to 24 slots of the runner's job
format, all 46 lanes of a problem in one slot, with paths relative to the
corpus root (the jobs' TPTP root); it stops on a split above its cap.
`--xmatrix` reads the harvested slots (`results/slot_K.tsv`, their
`.started` copies, `.done`), the OOM reruns (`results/rerun_K.tsv`) and
the pod records (`results/pods/<pod>/`: hostname, the kernel log's
OOM-killer lines, `--list-lanes=flags`), writes validate.txt (G0.1's bars:
completeness, sentinels and binary, crashes, OOM, row sanity, the lane
groups against the binary's flags) and, when no row is missing, the
tables of each split: T1-T4 and T8 as above, with the stem as the DOMNNN
prefix, and X10, which singles out the six lanes gated on the -10 shape
(horn-cd, horn-back and ifeq on an ifeq dispatcher; equals, narrowq and
equals-twee on a non-ground goal) against the Horn-release, Horn-free and
FOF groups.  `label_harvest_jobs.tsv` lists every TRAIN-X pair proved at
60 s.  `--xtwins` applies section 5.3's twin-family rule to TRAIN and
TRAIN-X together (amendment A24): the tail is TRAIN-T (`train_t.tsv`)
plus the TRAIN-X hard tail of `xmatrix.tsv`, each tail problem owned by
its fastest lanes, and the time per selection is T5's median over the
pooled TRAIN and TRAIN-X rows; horn-cd and horn-back, which search in
the CLI's Horn loops, are ineligible.  It writes the four families, the
replacement order and the other readings to `decisions_a24.txt` in the
X directory.  `test_matrix_tables.py` holds known-answer tests of the
three X modes on synthetic data (pods only).

The X matrix ran on 2026-10-06 (plan section 5.4, registry row "lane
matrix of the synthetic -10 corpus"): 85,560 of 85,560 rows on binary
11fad6a4cb94..., every validation item PASS, no crash and no OOM kill.
TRAIN-X: VBS 1,180 / 1,215 / 1,228 of 1,464 at 10 / 30 / 60 s, best
single lane twee-deep 972, union at k = 14, hard tail 48.  DEV-X: VBS
336 / 344 / 346 of 396, best twee 281, union at k = 8, hard tail 10.
The six gated lanes alone solve 140 TRAIN-X and 40 DEV-X problems.
`label_harvest_jobs.tsv` holds 19,913 TRAIN-X pairs on 1,228 problems,
8,348 of them front-end proofs.  Everything is in
`~/.cache/thvm/enigma/ifeq10/matrix/`.

The 180 s supplement reruns `supplement_jobs.tsv` with `runner.sh` at
`RUNNER_WALL=180 RUNNER_TIMEOUT=195`, its rows in
`supplement/sup_K.tsv` with their `.started` and `.done` sentinels.  Once
`supplement.tsv` exists, `--tables` adds `<table>_180_<split>.tsv` (T1-T4 of
the 180 s view: the matrix rows, each rerun job's 180 s row in place of its
60 s row) and `--decisions` reads that view wherever a section 5.3 rule reads
180 s: unique solves and the cover for P1 retirement, condition 2 of the
hard tail (DEV-T, TRAIN-T, the twin owners) and the M4c count.

The J13 rows stay sealed until the plan's final look.  After `--seal` every
mode reads `matrix.tsv` only and stops on a j13 row or on a path under the
sealed directory; the only j13 facts it prints are the sealed row count and
the archive sha256, and it checks that its outputs name no j13 problem.
Re-running `--seal` rebuilds both files from the sealed raw shards and checks
that they are byte-identical.  The job lists use the runner's shard format
(name, path, split, lane), so `runner.sh` takes them as they are.
`static_gates.py <tptp_root> <problem.p>...` prints the gate bits and live
lanes of the CLI's exit-fast lanes, the static replica that T7 checks.
`g04_dryrun.tsv` holds the 20 TRAIN problems of gate G0.4's dry run
(`tests/atp_sched_teardown.sh FINDPROOF TPTP_ROOT dryrun
tools/atp_enigma/g04_dryrun.tsv 180`); its header says how they were drawn,
and the plan's amendment A1 records its sha256.

Two pod scripts check the explicit-stack branch (enigma-ftdeep) before
it merges.  `crash_rows.sh <tptp_root> <out_dir>` reruns the 24
(problem, lane) rows that crashed in G0.1 (SWX233-1 on 14 lanes,
SYN986-10.005 / .006 on 5 each) with `--lane=NAME --szs --wall=60`,
once on the competition build under `ulimit -s 8192` and once on an
AddressSanitizer build under `ulimit -s 1024`, and fails on any signal
end or ASan report.  `ftdeep_pod.sh <main_tree> <branch_tree>
<tptp_root> <problems.txt> <out_dir>` runs the deep-term tests and
test_atp with their peak RSS, the OA / DN pins on both builds, and
paired main / branch lane runs (twee, wm, otter-lpo, tgt-sel) with
instruction counts and CPU time.

## ENIGMA selection layer: compiled-in heads (`weights_to_c.py`)

The guided twins' layer (`src/atp/guide.c`, [docs/atp/guide.md](../../docs/atp/guide.md))
reads its heads from `src/atp/guide_models.c`, which `weights_to_c.py`
generates from safetensors heads (metadata `Layout`, `Phi`, `Lanes`,
optional `LaneNames`, `Families`; a `topk_net.py export` file is a
`topk18` head). Pod-only:

```
weights_to_c.py build example=ex.safetensors \
  d32h64=models/topk_rerank_D32h64_dagger1.safetensors -o src/atp/guide_models.c
# engine: FindProof <base lane flags> --guide=<family> [--guide-model=d32h64] [--guide-zero]
# THVM_ATP_GUIDE_DUMP=<tsv> dumps every scored candidate's inputs and scores:
weights_to_c.py parity models/topk_rerank_D32h64_dagger1.safetensors dump.tsv out.txt
# the same run with THVM_ATP_HISTORY_EXPORT (RATE=1, CPFEAT=1), through np0_extract.py:
weights_to_c.py featparity run.npz dump.tsv out.txt
# a head's LaneNames against the binary's lanes:
FindProof --list-lanes > listed.txt; weights_to_c.py lanes head.safetensors listed.txt
# gates G2.2 / G2.4 / featparity over a spec of "family lane problem..." lines:
HEAD=head.safetensors MODEL=id FEATPARITY=1 tests/atp_guide_gates.sh bin/FindProof spec.txt
```

`weights_to_c.py example ex.safetensors` writes the tiny head the shipped
`src/atp/guide_models.c` is built from; rebuilding from it reproduces the
file byte for byte.

## History export, version 2

`THVM_ATP_HISTORY_EXPORT=<file>` (`THVM_ATP_HISTORY_RATE`, default 1/16)
makes one engine process append its derivation history to `<file>` as
64-byte records; the format is specified in the header of
[src/atp/history.c](../../src/atp/history.c). Version 2 (ENIGMA plan
section 3.2) adds to the version-1 stream the lane tag (`FINDPROOF_SLICE`,
set by `--lane=NAME`), the symbol table, the content of every rule as it
enters R (`Q` + `q`, read off the flatterm mirror), rule deaths, the Otter
replay and the selection dimension of each pick. One exporting process per
file: harvest with `--lane` (`np0_harvest.sh <FindProof> <TPTP> pool.tsv
<lane> <outdir>`, instruction clock, proved and failed runs alike);
`--schedule` gives each slice its own file, `<path>.<slice>`.

```
env THVM_ATP_HISTORY_EXPORT=run.hx THVM_ATP_HISTORY_RATE=1 THVM_ATP_WORK_CLOCK=instr \
  bin/FindProof --lane=twee --szs --proof --wall=180 problem.p > proof.out
np0_extract.py --stats run.hx                  # version, lane, record counts, picks
np0_extract.py --rules run.hx > rules.tsv      # every rule: kind, trace, uid, key, text
np0_extract.py run.hx proof.out run.npz        # labelled dataset (reads version 1 too)
np0_extract.py run.hx - run.npz                # failed run: hindsight labels only
np0_extract.py --selftest                      # reads synthetic v1 / v2 files back (pods)
```

In the `.npz`, `q_key` is each rule's canonical equation key (64-bit),
`rule_key` the key of the rule each selected CP became, and `proof_keys`
the keys of the rules in the proof's derivation DAG: matching one lane's
`proof_keys` against another lane's `rule_key` / `q_key` on the same
problem gives cross-lane labels, between lanes that share the input
transform. The export records the completion loop of every lane family;
the narrowing lanes' critical-goal queue and the CLI front ends (Horn
release, horn-cd chaining) are searches outside it and leave no records.

Gate G2.1 (the export only observes) is `tests/atp_history_v2_obs.sh`
(pods only) over `tests/atp_history_v2_obs.tsv` (10 TRAIN problems per
family, picked from the sealed lane matrix by `--pick`): each problem runs
with the export off, on and off again under the gate's step clock
(`THVM_ATP_WORK_CLOCK=steps`, which the export cannot advance). A row is
PASS (on = off and the export saw selections), NONDET (the two off runs
differ), UNOBSERVED (on = off but the export holds no selection), FAIL
(any other difference, a crash only with the export, or an export that
does not read back or was never closed) or WALL (a run reached the wall
first). A family passes when its first 7 conclusive rows are PASS and no
row FAILs (plan amendments A3 and A4).

In the `.npz`, `sig` fingerprints the session's input signature (the
`Y` records): cross-lane keys are compared only between equal `sig`s, and
`n_unnamed` counts function symbols without a name, whose keys cannot
match. `r_step` (each trace entry's creation step) is derived from the `X`
records, which carry the trace size at each pick.

## Synthetic -10 corpus (plan M2 deliverable 5, decision D1)

`FindProof --emit-ifeq10 problem.p` prints a CNF or FOF Horn problem in
TPTP's -10 unit-equality encoding ([CS18]) and exits.  It is a port of the
jukebox `horn-to-unit` pipeline that produced TPTP's -10 files (clausifier,
encoding and output naming, `tools/atp_cli/ifeq10.c`), written to
reproduce their dispatcher axioms, names, clause order and goal form.  On
the binary of main 6a4b244b the eleven reference cases match exactly, and
the ALL=1 survey, the held-out test of the port, matches all 158 further
[CS18] pairs outside the J13 stems: 152 exactly, 5 with other clause names,
1 with its ifeq conditions stacked in another order (gate G2.6, plan
section 5.4, 2026-10-06).  The FOF front end (`fof_prepass.c`)
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

| File | From the Mac | sha256 (plan amendment A21) |
|------|--------------|-----------------------------|
| j13_ueq_names.txt | ~/.cache/thvm/casc/J13/neural_p0/ | c43f0c4914bb4b295040f2a3186631da7a8955f991a81e5b15939d7b9cd69410 |
| pool.tsv | ~/.cache/thvm/casc/J13/neural_p0/ | 9fdb0135c608d6ed49e54b77dc94d20810ea4494aeb7974a4d75965181130594 |
| dev_t.tsv | ~/.cache/thvm/enigma/matrix/ | 4468a73697631ecfd0f973198d3879254efef504ca5a9734e5f6338819a8219b |
| train_t.tsv | ~/.cache/thvm/enigma/matrix/ | 6061641e745d437e6e1b5cdc36ac0f22b7def5c5a73628c81df71ec0e8c6fdd8 |

dev_t.tsv and train_t.tsv are the final DEV-T and TRAIN-T of the 180 s
decisions (`matrix_tables.py --decisions`, 7 problems each).

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
  devh_names.txt ~/.cache/thvm/enigma/matrix/dev_t.tsv \
  ~/.cache/thvm/enigma/matrix/train_t.tsv
```

walks the tree's CNF and FOF problems with status Unsatisfiable or Theorem
and writes `TRAIN-X/` and `DEV-X/`, each a TPTP-style root (`Problems/DOM/`,
emitted name = source name + `-10`) with a `pool.tsv` in `np0_pool.py`'s
columns, split 80/20 by `matrix_tables.py`'s `stem_hash` of the stem.
Skipped, with the reason in `manifest.tsv`: every stem of a J13 name (which
drops the -1/-2/-3 sources of J13's -10 theorems), every stem of an
exclusion list (above: DEV-H, DEV-T and TRAIN-T, so that no gate or tail
problem has an encoded copy in the corpus), problems that are unit equality already (by their SPC, or
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

The corpus that passed G2.6 (2026-10-06, binary 11fad6a4cb94..., the
registry row "synthetic -10 corpus" holds every sha256) is in
`~/.cache/thvm/enigma/ifeq10/corpus/`: TRAIN-X 1,464 problems from 36
domains, DEV-X 396 from 28, 53 encodings dropped as J13 duplicates.

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

## DEV-S, the manufactured band set (plan M2 deliverable 4, amendment A25)

DEV-S problems are ground goals the held-out runs derived: a rule that a
lane searching the untransformed input oriented in the second half of a
180 s run on a held-out problem, skolemized and posed against that
problem's axioms, and kept when P0 solves it in (W/2, 2W] at WDEP = 60 s or
at W180.  `devs.py` holds every reading (its header, amendment A25); its
inputs live in `~/.cache/thvm/enigma/devs/in/` (`sources.tsv`,
`source_lanes.txt`, `source_jobs.tsv`, `slots/`, `jobs.txt` with every
sha256).

```
devs.py lanes <(FindProof --list-lanes=flags)        # the source lanes (= source_lanes.txt)
devs.py jobs --problems problems.tsv --dev-t dev_t.tsv --train-t train_t.tsv \
  --lane-table lanes_flags.txt --slots 8 --out in/   # the source list and slots
# pod, per job: FindProof --lane=NAME --szs --wall=180 with the export at rate 1, then
devs.py extract --root tptp_pool --source NAME --path PATH --lane LANE --hx run.hx \
  --out cand/NAME__LANE.cand.tsv                     # candidate rows; delete run.hx
# Mac: the J13 goal check reads ~/.cache/thvm/casc/J13/UEQ statically and prints counts only
devs.py filter --cand cand/ --jobs in/source_jobs.tsv --pool-root tptp_pool \
  --pool-problems problems.tsv --corpus-root ~/.cache/thvm/enigma/ifeq10/corpus \
  --j13-dir ~/.cache/thvm/casc/J13/UEQ --out sel/   # problems, manifest, run_arms list
devs.py stage --problems sel/problems.tsv --rows S1/ --wall 60 --out s2.tsv    # 120 s list
devs.py stage --problems sel/problems.tsv --rows S1/ --wall 180 --out s3.tsv   # 360 s list
devs.py band --manifest sel/candidates.tsv --rows S1/ --rows S2/ --rows S3/ --out band/
devs.py gate --band band/ --rows AA/                 # G2.5 over the A/A pass
python3 tools/atp_enigma/test_devs.py -v             # synthetic exports and rows (pods)
```

`extract` alone needs numpy (it reads the export through `np0_extract.py`,
in chunks); it never reads a J13 file, and `filter` never sends one to a
pod.  A job keeps its 32 candidates of smallest rank (a stable hash of the
axiom system and the renaming-blind goal key, never difficulty); `filter`
drops J13 goals, goals of the pool and the synthetic corpus, duplicates,
the candidates past 10 per axiom system and past 1,000 in all, and names
the rest `<source>-S<k>`.

## Comparison protocol (`protocol/`)

The tools that run and judge every arm comparison of
[docs/plans/enigma_portfolio.md](../../docs/plans/enigma_portfolio.md)
(sections 1.3 to 1.5, gates G0.2 and G0.3, and the readings registered in
section 5.4).  Each file's header holds its exact definitions.

| file | what | where |
|---|---|---|
| `protocol/run_arms.py` | one comparison: every arm on every problem, one problem at a time, under the section 1.3 command, pod checks, calibration, shuffles, repeats and strict CheckProof; resumable, sentinels, sealed outputs for J13; `plan` prints a worst-case time estimate, `hostfacts` the pod's effective CPUs and memory | pod (`plan`: anywhere) |
| `protocol/calibrate_wdep.sh` | gate G0.3: the J13 submission build at 45, 53, 60 and 70 s, in shards, under the seal; `report` runs `stats.py wdep --look L0` | pod (report: Mac) |
| `protocol/stats.py` | `compare` (section 1.5), `delta` (D, delta, G0.2), `wdep` (G0.3); `--look L0`/`L1` print only the pre-registered aggregates and seal the per-problem tables under `~/.cache/thvm/enigma/sealed/`; `--look L2` needs the registry's unseal token | Mac (text only) |
| `protocol/calib_candidates.tsv` | the TRAIN problems the pod-speed reference is chosen from | |
| `protocol/test_stats.py` | known-answer tests on synthetic rows: `python3 tools/atp_enigma/protocol/test_stats.py` | Mac |

Pod rules (the runner refuses a pod that breaks one): 16 effective CPUs
(affinity mask, capped by a cgroup CPU quota); python3, coreutils `timeout`
and `lscpu`; one memory cap for the whole comparison, `--rss-cap default`
(the schedule's own 4500 MiB slice cap; needs an effective memory limit,
min(MemTotal, cgroup limit), of 16 x 4500 MiB) or `--rss-cap 3500` on every
pod of it; every arm on one binary.  Each arm's lane table comes from its
own binary (`<flags> --list-lanes=flags`, name TAB flags, which needs a build with
this listing); arms that differ in flags but list the same lanes are
refused unless declared (`--same-lanes PD,E`).  Inputs:
`~/.cache/thvm/casc/FindProof.tgz` (the submission build: the WDEP arm and
the pod-speed calibration binary),
`~/.cache/thvm/enigma/matrix/{j13_ueq.tgz,tptp_pool.tgz,problems.tsv}`, and
for current-main arms a source tree built with
`make -f tools/casc/Makefile bin/FindProof` and
`make -f tools/proover/Makefile bin/CheckProof`.

Driver (Mac side, one per pod, in the foreground): pick the shard count
from `run_arms.py plan` (it prints the worst case per shard and the shards
the deadline needs), launch with an absolute `--deadline-utc`, then poll
`out/run.done`, `out/run.failed` and `out/run.stop` up to twice the
worst-case estimate.  A `run.log` older than 45 minutes means the runner is
dead (sealed runs log a heartbeat every 15 minutes, open runs one line per
job).  On any sentinel or a dead runner: harvest (`tar czf` of the out
directory, copied to the Mac), then terminate the pod, then check
`prime pods list`.  A sealed out directory holds `sealed.tgz`; no agent
lists its contents (`tar t`, `ls -l`, `du` included): it opens only after
L2, or for a VerifiedBad under the section 1.3 exception.

WDEP calibration, 4 pods (`~/in` holds the four protocol files and the three
tarballs):

```
bash ~/in/calibrate_wdep.sh setup ~/wdep ~/in/FindProof.tgz ~/in/j13_ueq.tgz ~/in/tptp_pool.tgz
bash ~/wdep/bin/calibrate_wdep.sh calib-ref ~/wdep ~/in/calib_candidates.tsv   # pod 1 only, first
# record calib_ref.tsv's sha256 and lscpu line in the plan's section 5.4, copy the file to the
# Mac and to ~/wdep/ on the other pods, then on pod K:
nohup setsid bash ~/wdep/bin/calibrate_wdep.sh run ~/wdep K/4 ~/wdep/calib_ref.tsv \
  --deadline-utc YYYY-MM-DDTHH:MM:SSZ </dev/null >~/wdep/run_K.log 2>&1 &
# harvest: tar czf ~/wdep_K.tgz -C ~/wdep out_K setup.txt
bash tools/atp_enigma/protocol/calibrate_wdep.sh report -- ~/.cache/thvm/enigma/sealed/wdep_*.tgz
```

Before L0, the slice-stderr overhead (section 5.4): P0 on DEV-H at WDEP,
once as arm `P0e` and once as arm `P0n` with `--no-errdir` (two out
directories, same pod, same binary), then
`stats.py compare --rows OUT_E OUT_N --pair P0e,P0n`.

L0 (P0 at W180 and at the WDEP of G0.3, two passes; `arms.tsv` is
`P0<TAB>/root/l0/thvm/bin/FindProof<TAB>--schedule`, `j13.tsv` the `j13`
rows of the matrix `problems.tsv`, `calib_ref.tsv` the one table from the
WDEP calibration):

```
mkdir -p ~/sub ~/tptp_pool ~/j13
tar xzf ~/in/FindProof.tgz -C ~/sub; tar xzf ~/in/tptp_pool.tgz -C ~/tptp_pool
tar xzf ~/in/j13_ueq.tgz -C ~/j13; cp ~/in/run_arms.py ~/run_arms.snapshot.py
nohup setsid python3 ~/run_arms.snapshot.py run --problems ~/in/j13.tsv --arms ~/in/arms.tsv \
  --walls 180,WDEP --passes 2 --rss-cap default --root ~/j13 --checker ~/l0/thvm/bin/CheckProof \
  --calib-ref ~/in/calib_ref.tsv --calib-bin ~/sub/FindProof/bin/FindProof \
  --calib-root ~/tptp_pool --shard K/N --deadline-utc YYYY-MM-DDTHH:MM:SSZ --out ~/out_L0_K \
  </dev/null >~/L0_K.log 2>&1 &
```

A comparison of arms (L1, L2, DEV gates) is the same command with one line
per arm in `arms.tsv` and `--passes 1` (the default: repeats on
disagreements and clock-gated winners, read from each arm's own lane
table).  J13 output is never read: on the pod
`tar czf ~/L0_K.tgz -C ~ out_L0_K`, copy the archive into
`~/.cache/thvm/enigma/sealed/` on the Mac and leave it packed; `stats.py`
reads the archives (and the `sealed.tgz` inside) directly, all shards of a
look together:

```
python3 tools/atp_enigma/protocol/stats.py delta --look L0 --rows ~/.cache/thvm/enigma/sealed/L0_*.tgz
python3 tools/atp_enigma/protocol/stats.py compare --rows ~/.cache/thvm/enigma/sealed/L0_*.tgz \
  --pair P0@1,P0@2 --look L0 --label L0
python3 tools/atp_enigma/protocol/stats.py compare --rows ~/.cache/thvm/enigma/sealed/L1_*.tgz \
  --pair E,NL --pair E,PD --pair P1,P0 --nl P0,P1,PD --aa-rows ~/.cache/thvm/enigma/sealed/L0_*.tgz \
  --look L1 --label L1
```

Exit codes: `stats.py` 3 INVALID (incomplete cells or problems at a look,
mixed rss caps, incomplete A/A passes), 4 VerifiedBad (stop the line);
`run_arms.py run` 0 complete, 7 deadline (resume), 5 terminated, 4
VerifiedBad, 3 calibration failed, 2 preflight error or crash.

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
