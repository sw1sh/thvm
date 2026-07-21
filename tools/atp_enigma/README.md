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
