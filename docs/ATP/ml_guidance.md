# Machine-learned guidance for the equational ATP

How thvm uses machine learning (ML) to guide its automated theorem prover
(ATP), what the wider field does, and the concrete API + architecture that
ship today. Companion to the staged plan in
`docs/plans/atp_ml_roadmap.md` and the practitioner's tour in
`docs/tutorial/atp_methods.md`.

Abbreviations (spelled out on first use):

- ATP: automated theorem prover.
- CP: critical pair (a Knuth-Bendix overlap of two rules whose
  joinability must be checked); the unit the completion engine selects.
- ENIGMA: Efficient learNing-based Inference Guiding MAchine (Jakubuv and
  Urban's learned clause-selection line for the prover E).
- GBDT: gradient-boosted decision trees.
- GNN: graph neural network.
- KBO / LPO: Knuth-Bendix Ordering / Lexicographic Path Ordering, the two
  reduction orderings the engine ships.
- AUC: area under the ROC curve (a ranking-quality metric; 0.5 = random,
  1.0 = perfect).
- MLP: multi-layer perceptron.
- RL: reinforcement learning.
- ITP: interactive theorem prover (Coq / Lean / Isabelle / HOL).

## 1. The choice point ML attacks

A saturation prover runs the given-clause loop: it keeps a set of
processed facts and a queue of unprocessed ones, and at each step must
pick the next item to process. That pick -- clause selection -- is the
single most consequential heuristic decision in the prover. Pick well and
the empty clause (or the joined goal) is reached fast; pick badly and the
search space explodes.

thvm's engine is a Waldmeister-style unfailing Knuth-Bendix completion
prover, so its instance of that choice point is **critical-pair
selection**: which queued CP to orient and process next. The hand-tuned
answer is a weight function (`AtpCpWeightMode` in `src/atp/_.c`: ADD, GT,
MIX2, Twee, ConjSym, ...). The machine-learned answer is a model, trained
on past proofs, that scores each CP by its estimated proof-relevance and
biases selection toward the promising ones.

Everything below is about learning that selection. Completeness is never
at risk: the engine's periodic FIFO selection (the CPdimension /
`SelectionRatio` mechanism) fires regardless of the weight mode, so a
cold or over-fit model can only slow a proof, never lose one.

## 2. ENIGMA, concretely

ENIGMA (Jakubuv and Urban, CICM 2017) learns clause selection for E. The
training signal is the heart of the method and is dead simple:

1. Run the prover on a corpus of problems.
2. For each solved problem, label the clauses that ended up **in the
   proof** as positive and the clauses that were **processed but unused**
   as negative.
3. Train a classifier "good clause vs bad clause" over per-clause
   features.
4. Plug it back in as the selection weight -- usually *cooperatively*
   (blend the learned score with the standard age/weight heuristic),
   re-run the corpus, collect the new proofs, retrain. This iterated
   prove/label/retrain loop (MaLARea-style) is where the real gains come
   from, not any single model.

The model family evolved:

- **ENIGMA 2017** -- hand-crafted term-walk features (counts of symbol
  triples along term-tree paths) feeding a linear classifier.
- **ENIGMA-NG 2019** (arXiv:1903.03182) -- GBDTs (the workhorse: fast to
  evaluate, which matters because the prover scores thousands of
  clauses/sec) plus the first neural variants (recurrent nets over term
  trees).
- **ENIGMA Anonymous 2020** (arXiv:2002.05406) -- the big idea, symbol
  **anonymisation**: do not feed actual symbol names (they do not
  transfer across problems); represent a clause as a directed hypergraph
  (nodes = clauses, symbols, sub-terms/literals) and run a GNN whose
  output is invariant to symbol renaming. This generalises to problems
  with entirely unseen symbols.
- **Parental guidance + leakproofing** (arXiv:2102.13564) -- also filter
  *generated* clauses by their parents before they enter the queue, and
  an amortised evaluation server to hide GNN latency.

## 3. The wider ML-in-ATP landscape

Organised by where ML plugs in.

**A. Clause / CP selection in saturation provers** (the part thvm
implements):

- The ENIGMA family on E (above).
- **Deepire** (Suda, Vampire; arXiv:2102.03529) -- radically different:
  ignore the clause's logical content and score it purely from its
  *derivation history* (a tree-recurrent net over the derivation DAG).
  Cheap to evaluate, surprisingly strong, and orthogonal to content
  features, so it composes with them.
- **TRAIL** (IBM, AAAI 2021) and **Efficient Neural Clause-Selection
  Reinforcement** (Suda and Bartek, 2025; arXiv:2503.07792) -- frame
  selection as an RL problem and learn from proof success rather than
  imitating past proofs.

**B. Premise selection** -- pick relevant axioms/lemmas from a large
library *before* proving (the "hammer" problem): MaLARea, DeepMath
(arXiv:1606.04442), and recently Magnushammer (transformer contrastive
retrieval). Highest leverage when there is a big background theory; not
what thvm's engine needs for a self-contained equational problem.

**C. Representations** -- the through-line of the field: hand features ->
term-walk fingerprints -> tree-recurrent nets -> property-invariant GNNs
over formula hypergraphs (Olsak, Kaliszyk, Urban, ECAI 2020) ->
transformers.

**D. ITP tactic generation and LLM provers** -- a *different* track that
generates proof scripts for Coq / Lean / Isabelle (GPT-f, HyperTree Proof
Search, AlphaProof, DeepSeek-Prover). Powerful but not applicable to a
saturation engine; out of scope here.

A standing survey of where thvm sits relative to the field is in
`docs/atp/survey.md`.

## 4. What thvm ships

thvm trains and runs its guidance models with its own deep-learning stack
(`TFromNet` / `TNetTrain` / the tensor backend), so no external toolchain
is involved. The pieces, in dataflow order:

```
   corpus of theorems
        |
        v   TAtpCpDataset            (prove with feature recording on)
   labelled dataset  ----.           14 features x N rows, 0/1 labels
        |                |
        v   TAtpTrainScorer          (train via TNetTrain, thvm backend)
   model Assoc            \          linear or 1-hidden-layer MLP
        |                  \
        v   TAtpSetLearnedScorer      (push f64 blob into the C engine)
   engine has the model   /
        |                /
        v   TFindProof[..., Method -> "ENIGMA"]
   learned CP selection (completeness held by periodic FIFO)
```

### 4.1 Features (`ATP_CP_FEATURE_DIM = 14`)

`thvm_atp_cp_features` (`src/atp/_.c`) turns a CP `(lhs, rhs)` into a
fixed 14-vector. It is a pure read of the engine (rule set / goal / KBO
config); the same routine the runtime scorer calls at selection time.

| idx | name              | meaning |
|-----|-------------------|---------|
| 0   | `size_sum`        | symbol count of lhs + rhs |
| 1   | `max_depth`       | max term depth of the two sides |
| 2   | `n_distinct_vars` | distinct variables across lhs, rhs |
| 3   | `n_var_occ`       | total variable occurrences |
| 4   | `weight_add`      | ADD-mode weight (symbol-count sum) |
| 5   | `weight_gt`       | GT-mode weight (ordering-directed KBO) |
| 6   | `weight_mix2`     | MIX2-mode weight |
| 7   | `goal_weight`     | CPinGoal weight (0 in pure completion) |
| 8   | `age`             | birth order of the CP |
| 9   | `top_symbol_l`    | functor label at lhs root |
| 10  | `top_symbol_r`    | functor label at rhs root |
| 11  | `shares_goal_sub` | 1 if the CP touches a goal symbol |
| 12  | `orientable`      | 1 if the CP orients under KBO/LPO |
| 13  | `unif_measure`    | depth-weighted lhs/rhs disagreement |

These are deliberately content features the hand-tuned heuristics already
compute, so the model sees the same numbers they do. The symbol-name
features (9, 10) are the one thing that does NOT transfer across problems
with different signatures -- the limitation Tier 2's anonymising GNN
removes.

### 4.2 Labelling: proof-set reachability

After a successful proof, `thvm_atp_cp_label` marks each recorded CP 1 if
it is a transitive ancestor, in the recorded trace DAG, of the
goal-closing step, else 0. It seeds from the rules the proof extractor
says joined the goal, walks their `TRACE_ORIENT` lineage backward, and
flags every recorded CP whose trace id falls in that set. This is exact
dependency, not a size/age heuristic. Design note:
`docs/plans/atp_enigma_cp_selector.md`.

### 4.3 Dataset generation -- `TAtpCpDataset`

```wolfram
ds = TAtpCpDataset["GroupAxioms"]                 (* all NotableTheorems *)
ds = TAtpCpDataset[conjectures, axioms]           (* an explicit corpus *)
```

It points the env var `THVM_ATP_CP_DATASET` at a fresh file, proves the
corpus (each PROVED run records + labels + appends its CPs), and reads the
table back as
`<|"Features" -> n x 14, "Labels" -> 0/1, "FeatureNames" -> {...},
"NRows", "NPositive", "NProofs"|>`. Only proved problems contribute rows
(an unproved goal has no proof set to label against). Options: `Method`
(the base prover, default `{"Completion"}`), `TimeConstraint` per proof,
`MaxSteps`.

### 4.4 Training -- `TAtpTrainScorer`

```wolfram
r = TAtpTrainScorer[ds]                            (* or [theory] / [conjectures, axioms] *)
r["Model"]      (* the TAtpSetLearnedScorer-ready Association *)
r["TrainAUC"]   (* ranking quality on the training rows *)
```

A two-class softmax network (proof-relevant vs not) is trained through
thvm's own `TNetTrain`, then its output head is collapsed to the single
proof-relevance logit the engine ranks by (`z1 - z0`). Feature
standardisation (per-column mean / inverse-std from the data) is folded
into the model so the C side stays a plain forward pass. Options:
`"Hidden"` (0 = linear/logistic, >0 = one-hidden-layer ReLU MLP, default
16, max 64), `MaxTrainingRounds`, `"LearningRate"`, `"Method"` (the
optimiser, default `"Adam"`). The corpus forms (`TAtpTrainScorer[theory]`
/ `[conjectures, axioms]`) prep the dataset and train in one call.

### 4.5 The runtime scorer (C)

`thvm_atp_set_learned_scorer` (`src/atp/_.c`) loads a flat f64 parameter
blob into a process-global model used by CP weight mode 9
(`ATP_CP_WEIGHT_LEARNED`). Two kinds share one forward (`atp_learned_forward`):

- LINEAR: `score = b + W . z`
- MLP: `h = relu(W1 z + b1); score = b2 + W2 . h`

where `z = (feature - mean) * inv_std`. `score` is a raw logit (no sigmoid
head): higher means more proof-relevant means selected sooner, mapped to a
low heap priority. With no model pushed, a baked-in logistic regression
(`ATP_LEARNED_W` / `ATP_LEARNED_B`, held-out AUC ~0.85 on a notable-theorem
corpus) is the fallback. The bridge `thvm_wl_atp_set_learned_scorer`
(`wl/THVMLink/CSource/thvmlink_atp.c`) is what WL pushes through.

### 4.6 Using it -- `Method -> "ENIGMA"`

```wolfram
r = TAtpTrainScorer["GroupAxioms"];      (* prep + train *)
TAtpSetLearnedScorer[r["Model"]];        (* push *)
TFindProof[goal, axioms, Method -> "ENIGMA"]
```

`Method -> "ENIGMA"` is a first-class preset: learned CP selection on a
sound bounded-queue completion base (KBO + AutoPrecedence + UnfailingCP +
RHSInterreduce + AutoMaxWeight 20). It uses whatever model is currently
pushed -- the baked-in logistic regression until you push a trained one --
and never trains during search. `TAtpSetLearnedScorer[Clear]` reverts to
the baked-in scorer. Suboptions override the base, e.g.
`Method -> {"ENIGMA", "Ordering" -> "LPO"}`.

## 5. Measured result (honest)

A by-problem experiment (`/tmp/enigma_experiment.wls` is the harness): 36
single-conjunct problems from 6 tractable theories (AbelianGroup,
AbelianTarski, Boolean, Group, Huntington, Meredith), even/odd split into
18 train / 18 test. Dataset 608 labelled CPs / 149 positive; linear train
AUC 0.811 (an MLP at hidden width 16 overfit to AUC 0.61 on 14 features).

Held-out test, learned linear model vs a MATCHED control (the same
completion base with the hand-tuned GT weight instead of the learned one):

| metric | base (GT) | ENIGMA (learned) |
|--------|-----------|------------------|
| solved (of 18) | 11 | 11 |
| new solves | -- | none |
| regressions | -- | none |
| wall on 11 common solves | 0.49s | 2.24s |

A single-round linear model trained on ~18 problems does not crack any new
problems (a ranking AUC of 0.81 does not translate into more solves at
this scale), and *pure*-learned selection is markedly slower on the easy
problems. What the run does prove: the whole pipeline runs end-to-end on a
real multi-theory corpus, 72 proofs in one kernel session with no crash
(the flatterm multi-proof fix in `src/atp/ft_splice.c` held), solved-count
never regresses, and a wrong proof is never returned.

### 5.1 Coop fixes the regression and edges out the baseline

The literature's answer to "pure-learned is slow" is ENIGMA's *coop* mode:
select cooperatively with the base heuristic rather than by the model
alone. thvm has the mechanism -- the CPdimension secondary weight
(`thvm_atp_set_w2`): every k-th selection picks by a secondary weight
instead of the primary. Setting primary = learned (mode 9) and secondary =
GT, interleaved every 2nd pick, is coop. Re-measuring the same held-out
test set three ways:

| config | solved (of 18) | wall on 11 common solves |
|--------|----------------|--------------------------|
| base (GT)            | 11 | 0.51s |
| ENIGMA pure-learned  | 11 | 2.31s |
| **ENIGMA coop** (learned + GT every 2nd) | 11 | **0.39s** |

Coop is ~6x faster than pure-learned (it falls back to GT often enough to
avoid the pathological orderings the cold model picks) and modestly faster
than GT alone, at equal solved-count. The speed edge over GT is small-n
(11 mostly sub-0.1s proofs) and so suggestive rather than conclusive, but
the direction matches the literature exactly: coop is the configuration
that actually competes, pure-learned is not. The solved-count is still
unchanged -- cracking the hard hold-outs needs the bigger levers below.

Coop is a first-class Method suboption: `"CoopWeight"` (a
CriticalPairWeight name, the secondary weight) and `"CoopRatio"` (the
interleave ratio: every n-th selection uses the secondary). The `"ENIGMA"`
preset now **coops by default** (`"CoopWeight" -> "Gt", "CoopRatio" -> 2`);
pass `"CoopRatio" -> 0` for pure-learned. The suboptions thread through
`atpParseCompletionOpts` -> `cEngineProof` -> the proof bridge
(`thvm_wl_atp_run_proof`, args 31/32) -> `thvm_atp_set_w2`; the env vars
`THVM_ATP_W2_MODE` / `THVM_ATP_W2_MODULO` remain as a fallback.

How sharp the effect is on a single hard-for-the-model case
(`BooleanAxioms / AndPreAssociativity`, baked-in scorer):

| `Method -> "ENIGMA"` variant | wall |
|------------------------------|------|
| `"CoopRatio" -> 0` (pure-learned) | 7.48s |
| coop default (`"CoopRatio" -> 2`) | 0.02s |

The cold model alone wanders for 7.5s; interleaving a GT pick every other
step rescues it to 0.02s. This is exactly why pure-learned is the wrong
default and coop is the right one.

The levers that turn this into a win, in the literature and on the
roadmap:

1. **The learning loop.** Iterate prove/label/retrain so each round's new
   solves enlarge the next dataset. One round is not ENIGMA; the loop is.
2. **A much larger, more diverse corpus.** 18 problems is far below the
   thousands ENIGMA trains on.
3. **The symbol-independent GNN (Tier 2).** The hand features cannot
   generalise across signatures; the anonymising hypergraph GNN is what
   cracks unseen-symbol hold-outs in the literature.

## 6. Roadmap

Four tiers, built top-to-bottom; full detail in
`docs/plans/atp_ml_roadmap.md`.

1. **Neural scorer on the 14 features + the learning loop** -- BUILT
   (this document). Toolchain done, loop closes, measured baseline above.
2. **GNN on the term/clause hypergraph** (ENIGMA Anonymous) --
   symbol-independent, highest ceiling. The hard part is inference
   latency in the hot loop, which thvm's batched-tensor + JIT design is
   the right lever for (batch-score the queued CPs, compile the scorer to
   a fused kernel).
3. **Derivation-history network** (Deepire-style) over the trace DAG --
   cheap, orthogonal signal.
4. **RL clause selection** (TRAIL / neural-RL) -- learn from proof
   success; wants the Tier 1 loop in place first.

## References

- Jakubuv, Urban. ENIGMA: Efficient Learning-Based Inference Guiding
  Machine. CICM 2017.
- Chvalovsky, Jakubuv, Suda, Urban. ENIGMA-NG. CADE 2019.
  arXiv:1903.03182.
- Jakubuv et al. ENIGMA Anonymous: Symbol-Independent Inference Guiding
  Machine. IJCAR 2020. arXiv:2002.05406.
- Olsak, Kaliszyk, Urban. Property Invariant Embedding for Automated
  Reasoning. ECAI 2020.
- Suda. Vampire With a Brain Is a Good ITP Hammer (Deepire). 2021.
  arXiv:2102.03529.
- Goertzel et al. Fast and Slow Enigmas and Parental Guidance. 2021.
  arXiv:2102.13564.
- Suda, Bartek. Efficient Neural Clause-Selection Reinforcement. 2025.
  arXiv:2503.07792.
- Blaauwbroek et al. Learning Guided Automated Reasoning: A Brief Survey.
  2024. arXiv:2403.04017.
