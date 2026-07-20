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
