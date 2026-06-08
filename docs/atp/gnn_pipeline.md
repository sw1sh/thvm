# GNN guidance pipeline: dataset, training, and the ENIGMA Method

How to go from a corpus of equational problems to a trained graph neural
network (GNN) that guides thvm's automated theorem prover (ATP), and where
the open design problems are. This is the concrete, end-to-end "build it"
companion to the conceptual explainer in [ml_guidance.md](ml_guidance.md)
and the staged plan in [../plans/atp_ml_roadmap.md](../plans/atp_ml_roadmap.md)
/ [../plans/atp_tier2_gnn.md](../plans/atp_tier2_gnn.md).

Abbreviations on first use: ATP (automated theorem prover), CP (critical
pair, the Knuth-Bendix overlap the engine selects), GNN (graph neural
network), GCN (graph convolutional network), TPTP (Thousands of Problems
for Theorem Provers, the standard problem-file format), UEQ (unit
equality, the equational fragment), AUC (area under the ROC curve, a
ranking-quality metric where 0.5 is random), KBO (Knuth-Bendix ordering),
WM (Waldmeister). The acronym ENIGMA (Efficient learNing-based Inference
Guiding MAchine) is the learned clause-selection line this work ports.

## 1. The pipeline at a glance

```
  corpus                prove + label              dataset
  ------                -------------              -------
  AxiomaticTheory  -->  TFindProof (per goal)  --> TAtpGraphDataset
  raw TPTP File    -->  ProofObject + saturated     <|Graphs, Labels|>
                        rule set                          |
                                                          v
                                                   TAtpTrainGnn (GCN)
                                                          |
                                                          v
                              TAtpSaveGnnScorer --> .safetensors (paclet asset)
                                                          |
                                                          v
   TFindProof[goal, theory, Method -> "ENIGMA" + GNN coop]
        ^ GNN scores the secondary CP queue (cp_pri2); the WM heuristic
          still owns the primary heap; completeness held by periodic FIFO.
```

Three stages, each with its own design problems: build the dataset, train
the network, plug it into selection. Stage 3 (the in-engine scorer and the
coop wiring) already ships; stages 1 and 2 have tooling but no prepped
corpus-scale dataset yet.

## 2. Building the dataset

The unit of supervision is a labelled CP graph: a symbol-anonymized
structural encoding of one equation (a candidate lemma or critical pair),
labelled 1 if a proof actually used it and 0 if the saturation generated
it but the proof did not. A GNN learns "does this clause look
proof-relevant" from structure alone.

### 2.1 From AxiomaticTheory (the Wolfram corpus)

`TFindProof["thm", "theory"]` resolves names through `AxiomaticTheory`, and
`AxiomaticTheory[theory, "NotableTheorems"]` is the per-theory theorem
table. The dataset builders sit directly on top:

```wl
(* one theory: prove every NotableTheorem against its axioms, label *)
ds = TAtpGraphDataset["GroupAxioms"]
(* -> <|"Graphs" -> {TAtpCpGraph...}, "Labels" -> {0/1...},
        "NPos" -> p, "NNeg" -> n, "NProofs" -> k|> *)

(* an explicit conjecture set against shared axioms *)
ds = TAtpGraphDataset[conjectures, axioms]

(* the core source the above reduce to: graphs straight from proofs *)
ds = TAtpGraphDataset[{po1, po2, ...}]            (* positives only *)
ds = TAtpGraphDataset[po, TFindProof[g, ax, "Lemmas"]]  (* + negatives *)
```

Labelling (see the `TAtpGraphDataset` usage string for the exact rule):

- POSITIVES are the proof-essential lemmas: the equations of type
  `CriticalPairLemma` / `SubstitutionLemma` in the verified
  `ProofObject`'s proof chain (the lemmas the proof actually used).
- NEGATIVES are the saturated rule set (`TFindProof[..., "Lemmas"]`) minus
  any rule structurally equal to a positive.
- Structural equality uses a canonical key that renames pattern / Slot /
  free variables to positional placeholders in first-appearance order and
  treats the equation as an unordered pair, so `l == r` and `r == l`
  collapse. This both separates positives from negatives and drops
  duplicate rows.
- Only PROVED runs contribute graphs (an unproved goal has no proof set to
  label against). On the bundled corpus that is roughly 85 of 108
  NotableTheorems (see [../../tools/baselines/thvm_notable_theorems.tsv](../../tools/baselines/thvm_notable_theorems.tsv)).

A bare `ProofObject` carries only the positives; the saturated set is a
second, heavier proof projection. That asymmetry matters for the OOM
budget (Section 2.4).

### 2.2 From raw TPTP files

TPTP ingestion is already wired into `TFindProof` through the
`Wolfram`Parser`` `TPTPImport` parser (see
[../../wl/THVMLink/Kernel/ATP/TPTPImport.wl](../../wl/THVMLink/Kernel/ATP/TPTPImport.wl)):

```wl
po = TFindProof[File["problems/GRP001-1.p"]]   (* parse -> prove *)
po = TFindProof["cnf(c, axiom, f(X,Y)=f(Y,X)). ..."]   (* inline string *)
```

`TFindProof[File[...]]` reads and parses, proves the conjecture against the
axioms, and returns a `ProofObject`; feed that (and the saturated
`"Lemmas"`) to `TAtpGraphDataset[po, lemmas]` exactly as in 2.1. So a TPTP
directory becomes a dataset by:

```wl
files = FileNames["*.p", "tptp/UEQ"];
ds = TAtpGraphDataset @ DeleteCases[$Failed] @ Map[TFindProof[File[#]] &, files]
```

Scope and caveats:

- The parser handles the TPTP UEQ fragment (one equational literal per
  `cnf` clause). `fof` / `tff` / `thf` clauses and `include` directives are
  skipped with a console warning. The engine is equational, so non-UEQ
  problems are out of scope by construction.
- `Wolfram`Parser`` must be on the path. It can be absent or broken in
  headless / minimal kernels (it has poisoned a shared kernel in the docs
  build before), so a TPTP run should probe `TPTPImport` availability
  first and degrade gracefully.

### 2.3 The labelled CP graph (what the network sees)

Each graph is a `TAtpCpGraph` Association: an anonymized hypergraph of one
equation, built in C by `thvm_atp_cp_graph`
([../../src/atp/_.c](../../src/atp/_.c)). Nodes are a CP super-node, the
function-symbol occurrences, and the variable occurrences; edges are the
term structure (parent/child) plus the lhs/rhs split under the super-node.
Each node carries `ATP_CPG_FEAT_DIM` = 6 structural features (node-kind
one-hot over {CP super-node, symbol, variable}, occurrence count, side, and
so on). Crucially the features carry NO symbol identity. This is the
ENIGMA Anonymous design: the network generalizes across theories with
different signatures because it never sees "this is `nand`" versus "this is
`+`", only the shape. The Tier-1 hand-feature alternative (14 scalar
features per CP, `thvm_atp_cp_features`) is documented in
[../plans/atp_enigma_cp_selector.md](../plans/atp_enigma_cp_selector.md).

### 2.4 Design problem: OOM-safe generation at corpus scale

Building a dataset from a whole corpus means PROVING the whole corpus, and
proving is where the box is at risk. A diverging or pathological completion
can spin a WolframKernel to hundreds of GB of virtual memory and take the
operating system down; killing the wrapper script does not kill the kernel
child. So a naive `wolframscript` sweep over `AxiomaticTheory` or a TPTP
directory is the one thing not to do.

Mitigations, in order of safety:

1. Per-proof `TimeConstraint` (the builders default to 30s) AND a skip list
   for the known-divergent goals (the unproved ~23 of 108 add no positives
   anyway). Match the constraint to the recorded solve time plus a margin
   (the fast theorems prove in well under a second).
2. Chunk the corpus and use a FRESH kernel per chunk, so per-proof memory
   never accumulates across the whole sweep. Persist each chunk's dataset
   (`TSafeTensorSave` of the graph tensors, or a `.wxf` of the Association)
   and concatenate offline.
3. Monitor resident memory between chunks and abort on pressure rather than
   trusting the time bound alone.
4. For sweeps that only need timing or solve-status (not graphs), prefer
   the C-direct bench (`bin/test_atp_wolfram_bench`), which runs the engine
   with no kernel and so carries no kernel-OOM risk. The engine can record
   CP features and the trace-DAG relevance label in C
   (`thvm_atp_set_record_cp_features`, `thvm_atp_cp_label`,
   `thvm_atp_cp_graph`), so a C-direct dataset dump is feasible for any goal
   the bench can express; the gap is that the bench hardcodes a handful of
   goals and cannot enumerate the WL `AxiomaticTheory` corpus. Closing that
   gap (a `.pr` / TPTP reader in the bench, or a corpus encoding) is the
   clean long-term path to dataset generation that cannot crash the box.

### 2.5 Design problem: label definition and class balance

- Granularity. Tier 1 labels every PROCESSED critical pair (per-CP rows);
  Tier 2 labels per-lemma off the clean `ProofObject`. The per-lemma source
  is cleaner (it works even for a minimal-normal-form proof that records no
  per-CP features) but coarser. Whether to train on processed-CP rows or
  proof lemmas changes what "proof-relevant" means.
- Imbalance. Proofs use a handful of lemmas; saturation generates many. The
  positive rate is low and theory-dependent, so the objective and the AUC
  metric matter more than raw accuracy. Class weighting or a ranking loss
  may beat plain cross-entropy.
- Negative provenance. Negatives come from one saturation run; a different
  ordering or strategy saturates to a different rule set, so the "negative"
  label is strategy-relative, not absolute. Pooling negatives across
  strategies would reduce that bias.

### 2.6 Design problem: train/test split and leakage

- Split by PROBLEM, never by row: graphs from the same proof share
  structure and would leak across a random split. The Tier-2 held-out
  measurement (3 train theorems / 2 test theorems on GroupAxioms) reached
  test AUC 0.89, but that is within one theory.
- Signature leakage is mitigated by anonymization (the network cannot
  memorize symbol names), but structural idioms of a theory can still leak.
  The real test is cross-theory and cross-corpus generalization: train on
  one set of theories, evaluate solved-count on a disjoint set.

## 3. Training the GNN

### 3.1 Architecture and API

`TAtpTrainGnn` trains a GCN entirely in thvm's own tensor stack (the same
runtime that runs the deep-learning examples), so there is no external
framework dependency:

```wl
r = TAtpTrainGnn[ds, "Hidden" -> 32, "Rounds" -> 3, MaxTrainingRounds -> 300]
(* r["Model"], r["TrainAUC"], r["LossStart"], r["LossEnd"], r["NPos"], r["NNeg"] *)

r = TAtpTrainGnn["GroupAxioms"]          (* prep dataset + train in one call *)
r = TAtpTrainGnn[conjectures, axioms]
```

The forward batches every graph to a common padded node count, runs
`"Rounds"` rounds of row-normalized-adjacency message passing
(`H' = relu(A.H.W1 + H.Ws + b)`), masked-mean-pools to a graph embedding,
and reads out a two-class proof-relevance head trained with categorical
cross-entropy and Adam. `TrainAUC` is the Mann-Whitney rank AUC on the
training graphs. Defaults: `"Hidden"` 32, `"Rounds"` 3,
`MaxTrainingRounds` 300, `"LearningRate"` 0.01.

### 3.2 Save and ship

```wl
TAtpSaveGnnScorer[r["Model"], "gcn_atp.safetensors"]   (* -> .safetensors *)
m = TAtpLoadGnnScorer["gcn_atp.safetensors"]           (* round-trips *)
```

The weights become named tensors (`W1_<round>` / `Ws_<round>` / `Bh_<round>`
/ `Wout` / `Bout`); the scalar config (`Rounds` / `Hidden` / `NMax`) rides
in the file's `__metadata__`. The format is standard safetensors (tinygrad
`nn/state.py` layout), so the file is also readable by Python's
`safetensors`. A pretrained scorer ships as a paclet asset
(`wl/THVMLink/Assets/gcn_atp.safetensors`, registered as `GCNAtpScorer`);
`TAtpGnnScorerAsset[]` resolves its path. See
[../../wl/THVMLink/Kernel/SafeTensors.wl](../../wl/THVMLink/Kernel/SafeTensors.wl).

### 3.3 Design problems

- Model size versus the inference node cap. Inference truncates graphs to
  `ATP_GNN_N_CAP` = 64 nodes (Section 4.1). A network trained on much
  larger graphs would see truncated inputs at proof time; train near the
  inference distribution (`NMax`) or raise the cap with eyes open about the
  dense adjacency cost (it is O(N^2) per graph).
- Objective. Proof relevance is a ranking problem (select the best CP), not
  a calibrated probability; a pairwise / listwise ranking loss may track
  selection quality better than two-class cross-entropy.
- Scale. The held-out AUC came from one small theory. A corpus-scale
  dataset (Section 2) is the prerequisite for a scorer that generalizes;
  network capacity should grow with it.
- Provenance. A shipped asset should be reproducible from a recorded corpus
  and split, not an opaque blob. The current bundled asset predates a
  reproducible training script and is too weak to help on hard goals
  (Section 5.3); regenerating it from a defined corpus is the immediate
  next step.

## 4. Plugging into the ENIGMA Method

### 4.1 What ships today

The trained GCN runs as an in-engine scorer, in C, on thvm's tensor
runtime, with no WL round-trip in the proof loop:

```wl
TAtpSetGnnScorer[model]                       (* push a trained Model *)
TAtpSetGnnScorer["path.safetensors"]          (* or load from disk, lazy mmap *)
TAtpSetGnnScorer[TAtpGnnScorerAsset[]]        (* or the bundled asset *)
```

C-direct, the same is reachable through the bench for safe benchmarking:

```
THVM_ATP_WALDMEISTER=1 \
THVM_ATP_GNN_ASSET=wl/THVMLink/Assets/gcn_atp.safetensors \
THVM_ATP_GNN_RERANK_PERIOD=200 \
THVM_ATP_GNN_COOP_RATIO=4 \
  bin/test_atp_wolfram_bench mccune 300000 60
```

How it selects (the key design decision): the GNN drives the SECONDARY
selection dimension (`cp_pri2`), not the primary heap. Every
`w2_modulo`-th selection, `thvm_atp_select_cp` picks the GNN's top-scored
CP; all other selections use the primary heap, which stays the hand
heuristic (for example the Waldmeister MaxWeight / Mix weight). That is the
coop design real ENIGMA uses, and it is why the GNN can guide a competitive
preset instead of replacing it. Set with `thvm_atp_set_gnn_coop(s, ratio)`.
A not-yet-scored fresh CP gets a neutral secondary priority (the score-0
band) so it neither dominates nor starves the coop pick before the next
re-rank. Completeness is preserved: the engine's periodic FIFO selection
fires regardless of the score, so a cold or over-fit model only slows a
proof, never loses one.

Three engineering hazards were resolved to make this usable (see
[ml_guidance.md](ml_guidance.md) and the commit log): the per-re-rank cold
realize (solved by fixing the kernel shape so the CPU on-disk dylib cache
hits, no bespoke just-in-time capture needed); a crash on giant
out-of-distribution CP terms (the dense node-by-node adjacency blew memory;
fixed by capping the node dimension at 64, chunking the batch at 1024, and
running the forward in a sandbox tensor context that reclaims its scratch);
and the scratch leak that sandbox context closes.

### 4.2 Design problem: fold into `Method -> {"ENIGMA", "Model" -> ...}`

Today the GNN is enabled by a separate `TAtpSetGnnScorer` call plus the
coop knobs, not as a self-contained `Method`. `Method -> "ENIGMA"` selects
the Tier-1 LINEAR scorer (`CriticalPairWeight -> "Learned"`), not the GNN.
The target is one call:

```wl
TFindProof[goal, theory,
    Method -> {"ENIGMA", "Model" -> gnnModelOrPath, "CoopRatio" -> 4,
               "RerankPeriod" -> 200}]
```

The plumbing problem is that the WL Method runs the engine ATOMICALLY:
`thvm_wl_atp_run_proof` initializes, runs, and frees in one LibraryLink
call. The in-step re-rank hook already fires inside that atomic run, so the
Method dispatcher needs only to, BEFORE the run: (1) resolve `"Model"` (an
Association, a safetensors path, or the bundled asset) and push it via the
scorer C bridge; (2) set the re-rank period; (3) set `gnn_coop` and the
coop ratio. Concretely that means threading two or three new values
through `atpParseMethod` for the `{"ENIGMA", "Model" -> ..., ...}` subform,
into the option vector the loader passes to `cEngineProof`, and out to the
bridge arguments that call `thvm_atp_set_gnn_rerank_period` /
`thvm_atp_set_gnn_coop` (the existing CoopWeight / CoopRatio threading is
the template; it already widened the option vector once). Open sub-choices:
whether the GNN is a new `"ENIGMA"` subform or a sibling preset; whether
the model lives on the call or in process state; and whether the linear and
GNN scorers can coop simultaneously (they would contend for the single w2
dimension, so a second secondary dimension would be needed).

### 4.3 Design problem: when does it actually help

Selection-order guidance only helps if the scorer is good AND the coop
ratio is tuned. Measured on the bench with the current (weak, unprovenanced)
asset and the Waldmeister primary:

| goal     | WM baseline      | + GNN coop r=2      | + GNN coop r=10   |
| -------- | ---------------- | ------------------- | ----------------- |
| `thm`    | 207 steps / 0.4s | 104 steps / 0.2s    | 165 steps / 0.4s  |
| `mccune` | 5427 steps / 8.5s| timeout             | 9091 steps / 38s  |

The mechanism works and is now benchmarkable against a competitive preset,
but this asset helps only on the trivial goal and derails the tuned search
on the hard one at every ratio. Two levers: scorer quality (Sections 2-3,
the gating factor) and the coop ratio (gentler is safer; the primary heap
stays in control). The honest reading is that a useful result needs a
properly trained scorer first, then a ratio sweep, then a held-out
solved-count comparison over the corpus, using the C-direct bench so the
sweep cannot crash the box.

## 5. End-to-end recipe (once a reproducible corpus exists)

```wl
(* 1. dataset: chunk the corpus, fresh kernel per chunk, persist each *)
ds = TAtpGraphDataset[trainTheories]            (* OOM-safe, see 2.4 *)

(* 2. train + hold out by problem *)
r = TAtpTrainGnn[ds, "Hidden" -> 32, "Rounds" -> 3]

(* 3. ship *)
TAtpSaveGnnScorer[r["Model"], TAtpGnnScorerAsset[]]

(* 4. evaluate solved-count on a disjoint corpus, C-direct, ratio swept *)
(*    bin/test_atp_wolfram_bench <goal> with THVM_ATP_GNN_COOP_RATIO in {2,4,8} *)
```

## 6. Open problems checklist

- [ ] OOM-safe corpus-scale dataset generation (chunked kernels, or a
      C-direct corpus reader so generation never touches a kernel).
- [ ] A reproducible training script with a recorded corpus and split;
      regenerate the bundled asset from it.
- [ ] `Method -> {"ENIGMA", "Model" -> ...}` fold (atomic-run plumbing).
- [ ] Ranking objective and class-imbalance handling in training.
- [ ] Held-out solved-count over a disjoint corpus, with a coop-ratio sweep.
- [ ] Cross-theory and cross-corpus (TPTP) generalization measurement.

## References

See [ml_guidance.md](ml_guidance.md) for the full reference list (ENIGMA,
ENIGMA-NG, ENIGMA Anonymous, Deepire, the neural clause-selection
reinforcement work, and the learning-guided-reasoning survey).
