# Machine-learned guidance for the equational ATP: roadmap

thvm's Waldmeister-style equational automated theorem prover (ATP) already
ships the *data foundation* for ENIGMA-style learned critical-pair (CP)
selection: a 14-feature per-CP extractor, a trace-DAG proof-relevance
labeller, a TSV dataset exporter, and a baked-in logistic-regression scorer
wired in as `ATP_CP_WEIGHT_LEARNED` (CP-selection mode 9). See the design
note [atp_enigma_cp_selector.md](atp_enigma_cp_selector.md) for that layer.

This document is the roadmap for the *neural* tiers we build on top, in
order. It exists because thvm already has a capable deep-learning stack
(`TFromNet` / `TNetTrain` / the tinygrad-ported tensor backend), so the
prover can train and run its own guidance models without an external
toolchain.

For ENIGMA itself and the wider field, the short version: ENIGMA (Jakubuv
and Urban, CADE 2017) learns, from solved problems, a classifier over
per-clause features that predicts proof-relevance and biases given-clause
selection -- the single most important choice point in a saturation prover.
It evolved from hand features + linear/gradient-boosted trees (2017-2019)
to symbol-independent graph neural networks with anonymisation (ENIGMA
Anonymous, IJCAR 2020). The parallel lines are Deepire (Suda; a tree neural
network over the derivation history, ignoring clause content) and
reinforcement-learning clause selection (TRAIL; and the 2025 neural-RL
work). Our CP queue is the completion-calculus instance of the same
given-clause choice point, so all of these map onto it.

A property that holds across every tier and de-risks the whole effort: the
engine's periodic FIFO selection (CPdimension / `SelectionRatio`) fires
regardless of the weight mode, so *no* learned scorer can break refutational
completeness. We can experiment freely; a cold or over-fit model can only
slow a proof, never lose one.

## Current state (Tier 0, done)

- Feature extractor `thvm_atp_cp_features` (14 dims), `src/atp/_.c`.
- Proof-relevance labeller `thvm_atp_cp_label` (trace-DAG reachability).
- TSV export `thvm_atp_cp_dataset_append`, env-gated via
  `THVM_ATP_CP_DATASET` in the WL proof bridge
  (`wl/THVMLink/CSource/thvmlink_atp.c`).
- Baked-in logistic-regression scorer `atp_cp_learned_priority`
  (`ATP_LEARNED_W[14]`, `ATP_LEARNED_B`), held-out AUC ~0.85 on the 83
  AxiomaticTheory NotableTheorems.
- WL exposes `"CriticalPairWeight" -> "Learned"` (`wl/.../ATP/ATP.wl`).

The gap was: the scorer weights were compile-time constants and there was
no in-WL path to generate a dataset, train, reload, and measure. Tier 1
closes that loop and replaces the linear model with a neural one.

**Done so far -- the whole Tier 1 toolchain is built and the loop closes
end-to-end** (validated on a small group-theory corpus; a real win needs a
full corpus, see step 4):

- **Step 2, runtime-loadable scorer.** The file-static `const` weights are
  now a fallback; a model pushed from WL via `TAtpSetLearnedScorer[model]`
  (LINEAR or one-hidden-layer ReLU MLP, H <= 64) takes over
  `ATP_CP_WEIGHT_LEARNED` with no recompile. C side:
  `thvm_atp_set_learned_scorer` / `thvm_atp_clear_learned_scorer` +
  `atp_learned_forward` in `src/atp/_.c`; bridge
  `thvm_wl_atp_set_learned_scorer` in `wl/THVMLink/CSource/thvmlink_atp.c`.
  Features are standardized (mean / inv_std shipped with the model) then run
  through a plain forward emitting a raw logit, reusing the existing
  score -> priority mapping. Tests: `tests/test_atp_enigma.c` (forward
  hand-checks, linear-reproduces-baked-in, malformed rejection) +
  `ATP/enigma/*` in `wl/THVMLink/Tests/atp.wlt`.
- **Step 1, dataset surface.** `TAtpCpDataset[conjectures, axioms]` /
  `TAtpCpDataset[theory]` proves a corpus with recording on and returns
  `<|"Features" -> n x 14, "Labels" -> 0/1, ...|>` (env-gated TSV capture,
  read back + parsed). `wl/THVMLink/Kernel/ATP/ATP.wl`.
- **Step 3, training.** `TAtpTrainScorer[dataset]` trains a two-class
  softmax net through thvm's own `TNetTrain`, collapses the head to the
  single proof-relevance logit, folds in standardization, and returns the
  `TAtpSetLearnedScorer`-ready model + a train AUC. Convenience overloads
  `TAtpTrainScorer[theory]` / `TAtpTrainScorer[conjectures, axioms]` prep
  the dataset and train in one call. `ATP.wl`.
- **First-class Method.** `Method -> "ENIGMA"` is a registered preset
  (learned CP selection on a KBO + AutoPrecedence + UnfailingCP +
  RHSInterreduce + AutoMaxWeight 20 base), so
  `TFindProof[conj, ax, Method -> "ENIGMA"]` runs the learned scorer (the
  pushed model, or the baked-in logistic regression) without the explicit
  `{"Completion", "CriticalPairWeight" -> "Learned"}` spec. Tests:
  `ATP/enigma/method-preset-*` in `atp.wlt`. Full user flow:
  `r = TAtpTrainScorer[theory]; TAtpSetLearnedScorer[r["Model"]];
  TFindProof[g, ax, Method -> "ENIGMA"]`.
- **Prerequisite bug fixed.** Generating a dataset requires many proofs in
  one process, which exposed a pre-existing flatterm double-free
  (`ft_alloc: free_span underflow`): `ft_splice_inplace_const_or_unbound_var`
  collapsed an inner subtree without bubbling ancestor `->end` pointers
  past the freed span. Fixed in `src/atp/ft_splice.c` (mirrors the sibling
  regime's bubble-up); regression test `tests/test_atp_multiproof.c`;
  `test_atp` stays 135623/135623 with FT normal forms byte-consistent.
- **Loop close, validated.** prove -> `TAtpCpDataset` -> `TAtpTrainScorer`
  -> `TAtpSetLearnedScorer` -> reprove runs end-to-end. On a 12-row toy
  corpus the trained model reaches AUC ~0.89 but (correctly) steers search
  worse than the default -- expected for a 12-row model; it does not crash
  and never returns a wrong proof (FIFO fairness keeps completeness).
- **Step 4, measured (honest: no win yet).** Real multi-theory corpus,
  36 single-conjunct problems from 6 tractable theories (AbelianGroup,
  AbelianTarski, Boolean, Group, Huntington, Meredith), by-problem even/odd
  split (18 train / 18 test). Dataset 608 labelled CPs / 149 positive;
  linear train AUC 0.811 (MLP 0.608, overfit at H=16 on 14 features).
  Held-out test, learned linear vs a MATCHED control (same completion
  base, hand-tuned GT weight): solved 11 = 11, no new solves, no
  regressions, learned ~4.5x slower on the 11 common solves (0.49s ->
  2.24s, one outlier). 72 proofs ran in one kernel with no crash (the
  flatterm fix held). Conclusion matches the ENIGMA literature: a
  single-round linear model on ~18 problems does not beat a tuned
  heuristic; ranking AUC does not equal a search win at this scale.
  Levers for an actual win: (a) the learning LOOP -- iterate
  prove/label/retrain so each round's solves feed the next dataset;
  (b) a much larger corpus; (c) Tier 2's symbol-independent GNN, which is
  what cracks the hard hold-outs in the literature. Harness:
  `/tmp/enigma_experiment.wls` (reproducible; tighten the corpus + raise
  TimeConstraint to scale it).
- **Coop closes the regression + edges out the baseline.** Real ENIGMA is
  *coop* (learned interleaved with the base heuristic), not pure-learned.
  Wired via the engine's CPdimension secondary weight
  (`thvm_atp_set_w2`), env-gated in the bridge by `THVM_ATP_W2_MODE` +
  `THVM_ATP_W2_MODULO`. Same held-out test, three ways: solved 11 = 11 =
  11; wall on the 11 common solves base(GT) 0.51s, pure-learned 2.31s,
  **coop (learned + GT every 2nd pick) 0.39s** -- ~6x faster than
  pure-learned and modestly under GT, at equal solved-count (small-n, so
  suggestive). Confirms the literature: pure-learned underperforms, coop
  competes. Harness `/tmp/enigma_coop.wls`. Follow-up: promote coop to
  first-class `Method -> {"ENIGMA", "CoopWeight" -> "Gt", "CoopRatio" ->
  2}` suboptions (threads 2 args through the bridge) so the preset coops
  by default.

## Tier 1: neural MLP / GBDT on the existing features + the learning loop

Goal: beat the AUC ~0.85 baseline and, more importantly, beat the
hand-tuned heuristics on *problems solved* under a fixed budget -- using the
14 features already extracted, a thvm-trained model, and a closed
prove -> label -> train -> reload -> reprove loop.

Sub-steps:

1. **WL dataset surface.** A `TAtpDataset[specs, opts]`-style entry that
   runs a corpus (default: the 83 provable NotableTheorems) with feature
   recording on, and returns the labelled matrix (one row = label + 14
   features) to WL, in addition to / instead of the TSV side-channel.
   Wraps the existing env-gated capture; mostly WL plus, if needed, a small
   LibraryLink to read `cp_feat_rows` / `cp_feat_label` back without a file.

2. **Runtime-loadable scorer + MLP forward (C).** Replace the file-static
   `const` weights with a settable parameter block on `AtpState` (or a
   process global the bridge sets before a run), and add a one-hidden-layer
   MLP forward (`h = relu(W1 x + b1); score = W2 h + b2`), allocation-free
   with fixed scratch. The linear case must reproduce the current baked-in
   logreg bit-for-bit so Tier 0 stays the fallback. A LibraryLink entry
   pushes trained parameters from WL.

3. **Train in thvm.** `TNetTrain` an MLP (`14 -> H -> 1`, ReLU, sigmoid
   head) on the dataset; class-imbalance-weighted; split by *problem* not by
   row to avoid leakage; report AUC / precision-at-k (selection only needs a
   good ranking). Export the parameters in the layout step 2 consumes. A
   gradient-boosted-tree variant is an alternative head distilled to the
   same C evaluator.

4. **Close + measure the loop.** Prove corpus -> dataset -> train -> reload
   -> reprove. Report solved-count delta vs the hand-tuned default and vs
   the Tier 0 linear scorer, under a fixed step/time budget. Iterate the
   loop (each round's proofs feed the next training set) until it plateaus.

Why first: it reuses the entire Tier 0 pipeline, the only genuinely new
infra is "push trained weights into the running engine", and it validates
the end-to-end loop and measurement harness that every later tier needs.

## Tier 2: graph neural network on the term / clause hypergraph

The real ENIGMA Anonymous: represent a CP as a directed hypergraph over
clause / symbol / sub-term nodes, run a message-passing GNN whose output is
invariant to symbol renaming, and score CPs by the embedding. Generalises to
problems with unseen symbols, which the 14 hand features cannot.

- The discrimination-tree index already flattens terms to integer symbol
  sequences (`src/atp/_.c`), so the graph is half-built; the work is the
  hypergraph construction + a thvm GNN (message passing as gather/scatter +
  matmul) + anonymised symbol embeddings.
- The hard problem is *inference latency in the hot loop*: the engine scores
  thousands of CPs/sec, and per-CP GNN inference is the bottleneck ENIGMA
  solved with an amortised evaluation server + a cheap "parental guidance"
  pre-filter. thvm's batched-tensor + JIT design is the right lever:
  batch-score the queued CPs (the heap already holds them) and compile the
  scorer to a fused kernel. Design for batching from the start.

## Tier 3: derivation-history network (Deepire-style)

Score a CP from its *derivation history* (the trace DAG already records
this) rather than its term content -- a tree/graph network over the lineage.
Inference is very cheap (structure only), and the signal is orthogonal to
the content features, so it composes with Tiers 1-2 rather than replacing
them.

## Tier 4: reinforcement-learning clause selection

Frame CP selection as a Markov decision process and learn from proof success
(TRAIL; the 2025 efficient neural clause-selection RL work) instead of
imitating past proofs. Highest novelty and the hardest to stabilise; wants
the Tier 1 loop, dataset surface, and runtime-loadable scorer already in
place, so it is last.

## References

- Jakubuv, Urban. ENIGMA: Efficient Learning-Based Inference Guiding
  Machine. CICM 2017.
- Chvalovsky, Jakubuv, Suda, Urban. ENIGMA-NG: Efficient Neural and
  Gradient-Boosted Inference Guidance for E. CADE 2019. arXiv:1903.03182.
- Jakubuv et al. ENIGMA Anonymous: Symbol-Independent Inference Guiding
  Machine. IJCAR 2020. arXiv:2002.05406.
- Olsak, Kaliszyk, Urban. Property Invariant Embedding for Automated
  Reasoning. ECAI 2020.
- Suda. Vampire With a Brain Is a Good ITP Hammer (Deepire). 2021.
  arXiv:2102.03529.
- Suda, Bartek. Efficient Neural Clause-Selection Reinforcement. 2025.
  arXiv:2503.07792.
- Blaauwbroek et al. Learning Guided Automated Reasoning: A Brief Survey.
  2024. arXiv:2403.04017.
