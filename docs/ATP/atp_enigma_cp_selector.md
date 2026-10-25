# ENIGMA-style learned critical-pair selector for the equational ATP

This is the design + training note for a learned critical-pair (CP)
selector in thvm's Waldmeister-style equational automated theorem prover
(ATP). The selector is the *machine-learned* analogue of the hand-tuned
`ClasHeuristics` weight functions (`AtpCpWeightMode`): instead of a fixed
formula picking which queued CP to process next, a model trained on past
proofs scores each CP by its estimated proof-relevance.

ENIGMA (Jakubuv & Urban, "ENIGMA: Efficient Learning-based Inference
Guiding MAchine", CADE 2017) does exactly this for the saturation prover
E: it learns, from solved problems, a classifier over per-clause feature
vectors that predicts whether a given clause is proof-relevant, and uses
it to bias clause selection. We adapt the idea to *critical pairs* in
unfailing completion.

This document covers the **data-foundation step only** (implemented):
how a successful proof exports a labelled dataset. Training the model and
compiling a fast C scorer into CP selection are later steps (sketched at
the end).

## 1. What gets recorded

When `s->record_cp_features` is set (via
`thvm_atp_set_record_cp_features(s, 1)`; **default OFF**), every CP that
`thvm_atp_select_cp` pops from the queue -- i.e. every CP the engine
actually **processed**, not merely generated -- appends one row to
growable arrays on the `AtpState`:

- `cp_feat_rows[i * ATP_CP_FEATURE_DIM .. ]` -- the feature vector (f32),
- `cp_feat_trace[i]` -- the CP's trace-entry index (its lineage handle),
- `cp_feat_label[i]` -- the binary label, filled post-hoc (see section 3).

With the flag off, `select_cp` runs a single predictable-branch test and
records nothing; the engine is byte-identical to the untracked run
(`make bin/test_atp && ./bin/test_atp` stays at its fixed check count).

## 2. Feature schema (`ATP_CP_FEATURE_DIM = 14`)

`thvm_atp_cp_features(s, lhs, rhs, age, out)` fills `out[0..14)`. It is a
pure read of the engine (rule set / goal / KBO config) and is safe to
call standalone -- the same routine a future C scorer would call at
selection time.

| idx | name              | meaning |
|-----|-------------------|---------|
| 0   | `size_sum`        | `symbol_count(l) + symbol_count(r)` (the ADD weight's term mass) |
| 1   | `max_depth`       | `max(term_depth(l), term_depth(r))` |
| 2   | `n_distinct_vars` | distinct FVR ids across `l` and `r` |
| 3   | `n_var_occ`       | total FVR occurrences across `l` and `r` |
| 4   | `weight_add`      | ADD-mode priority (symbol-count sum) |
| 5   | `weight_gt`       | GT-mode priority (ordering-directed KBO weight; engine default heuristic) |
| 6   | `weight_mix2`     | MIX2-mode priority (`g*10 + (wl+wr)`) |
| 7   | `goal_weight`     | CPinGoal weight (`atp_goal_weight`); 0 in completion mode |
| 8   | `age`             | birth order of the row (== its index in the recording arrays) |
| 9   | `top_symbol_l`    | CTR label at `l`'s root (0 if a var/atom) |
| 10  | `top_symbol_r`    | CTR label at `r`'s root |
| 11  | `shares_goal_sub` | 1 if `l` or `r` shares a function symbol with the (normalized) goal, else 0 |
| 12  | `orientable`      | 1 if the CP orients under KBO/LPO (`KBO_GT`/`KBO_LT`), else 0 |
| 13  | `unif_measure`    | depth-weighted l/r disagreement (Waldmeister `U1_Unifikationsmass`) |

Features 4-6 reuse the exact `atp_cp_weight_base` formulas the engine
already computes, so the model sees the same numbers the hand-tuned
heuristics do. Feature 7 reuses `atp_goal_weight` (the CPinGoal
classifier). The fixed length keeps the dataset a plain numeric matrix.

### Approximations (honest list)

- **`shares_goal_sub` (feature 11)** is a *symbol-set overlap* proxy for
  ENIGMA's goal-distance feature, not a positional/structural match. It
  asks only "does this CP touch any function symbol the goal uses?". A
  finer feature (e.g. count of shared subterm patterns, or the CPinGoal
  coverage value) could replace it; feature 7 already carries the precise
  CPinGoal residual, so 11 is a cheap complementary binary signal.
- **`n_distinct_vars` (feature 2)** uses a 64-bit membership bitset keyed
  on `var_id & 63`. Variable ids are dense `[0, k)` after `ATP_VAR_NORM`
  on the canonical CP forms this records, so the count is exact for the
  CPs the engine produces (k is far below 64); it would only undercount
  on a pathological non-normalized term with >64 distinct vars.
- Everything else (sizes, depths, weights, orientability, unification
  measure, top symbols, age, goal weight) is exact -- a direct call into
  the engine's own helpers.

## 3. Labels: proof-set reachability (not a heuristic)

After a **successful** proof, `thvm_atp_cp_label(s)` assigns each recorded
row a binary label using the **actual proof dependency**, computed by the
same machinery the proof extractor uses:

1. `thvm_atp_proof_extract` reconstructs the equational rewrite chain that
   joins the two goal sides under the final rule set R. It returns the
   **rules** that fire to join the goal -- the proof's rule dependencies.
2. Each fired rule `ru` has `s->r_trace[ru]`: the `TRACE_ORIENT` trace
   entry that birthed it.
3. Starting from those `TRACE_ORIENT` ids, we take the **transitive
   parent closure** over the trace DAG (`s->trace[]`). Parents are read
   off each entry's children: for `TRACE_AXIOM` / `TRACE_ORIENT` /
   `TRACE_CP` / `TRACE_SIMPLIFY`, children 0 and 1 are parent trace ids;
   for `TRACE_NORM_STEP`, only child 0 is a parent (child 1 is a *rule
   index*, not a trace id). This is `atp_trace_parents`.
4. A recorded selected-CP is labelled **1** iff its `cp_feat_trace[i]` is
   in that reachable set, else **0**.

`thvm_atp_cp_label` returns the count of distinct proof-relevant selected
CPs (the proof-set size), which is `<=` the number of recorded rows.

This is exact trace-DAG reachability from the goal-closing step, not a
size/age heuristic: a CP is positive precisely when one of the proof's
rules descends from it in the recorded lineage. The one caveat is the
same as `thvm_atp_proof_extract`'s: a goal closed *only* by the MNF
bidirectional search (a symmetric conjecture with no shared single normal
form) is not single-NF extractable, so `thvm_atp_cp_label` returns 0 for
it (no proof set -> no positive labels). For the single-NF provable
corpus (the 83 AxiomaticTheory notable theorems) this is the right and
complete dependency.

To get the CP -> ORIENT lineage in the trace DAG, run with
`thvm_atp_set_record_norm_steps(s, 1)` -- the same mode the WL proof
extractor uses. Without it the `TRACE_ORIENT` parent chain still reaches
the source CP, but norm-step recording is the tested configuration.

## 4. Export format + how to trigger

`thvm_atp_cp_dataset_append(s, path, header)` appends the labelled rows to
a TSV file (created if absent). Pass `header = 1` only for the first proof
in an accumulating run; `0` thereafter. One row per recorded CP:

```
label \t size_sum \t max_depth \t n_distinct_vars \t n_var_occ \t \
  weight_add \t weight_gt \t weight_mix2 \t goal_weight \t age \t \
  top_symbol_l \t top_symbol_r \t shares_goal_sub \t orientable \t unif_measure
```

Driver loop for the corpus (C; a WL `LibraryFunction` wrapper over the
three entry points is the natural surface):

```c
const char *path = "enigma_cp_dataset.tsv";
for (each provable goal g in the corpus) {
    AtpState *s = thvm_atp_init(cfg, step_cap);
    thvm_atp_set_record_norm_steps(s, 1);
    thvm_atp_set_record_cp_features(s, 1);
    thvm_atp_set_goal(s, g.lhs, g.rhs);
    /* add g's axioms ... */
    if (thvm_atp_run(s) == ATP_PROVED) {
        thvm_atp_cp_label(s);                    /* fill labels */
        thvm_atp_cp_dataset_append(s, path, first); /* header once */
        first = 0;
    }
    thvm_atp_free(s);
}
```

Running the 83 currently-provable AxiomaticTheory notable theorems this
way accumulates a few thousand labelled CPs into one TSV, ready for
training.

## 5. Training + integration path (later step, NOT in C)

Train off-line in Python or WL on the TSV:

- **Logistic regression** (cheap, interpretable) or a **gradient-boosted
  decision tree** (GBDT, e.g. XGBoost/LightGBM) over the 14 features,
  target = `label`. The dataset is class-imbalanced (few positives per
  proof), so weight the positive class or use `scale_pos_weight`.
- Standard split by *problem* (not by row) to avoid leakage; report
  AUC / precision-at-k, since selection only needs a good *ranking*.

Integrate the learned scorer back into selection:

- For logistic regression, the model is just a weight vector `w[14]` + bias
  `b`. A C scorer `atp_cp_score(s, l, r, age) = dot(w, features) + b`
  drops straight into `thvm_atp_select_cp` as an alternative key (or as a
  blend with `cp_pri`), gated by a new `AtpCpWeightMode` (e.g.
  `ATP_CP_WEIGHT_LEARNED`). The feature extractor `thvm_atp_cp_features`
  is already the exact function the scorer would call.
- For a GBDT, export the trees to a small C evaluator (or distil to a
  linear model) so selection stays allocation-free in the hot loop.
- Keep it ENIGMA-anytime style: blend the learned score with the existing
  GT weight so a cold/over-fit model can't starve the queue (preserving
  completeness via the existing FIFO/`auto_max_cp_weight` fairness).

## 6. Test

`tests/test_atp_enigma.c` runs the headline group-axiom proof
(`f(a, i(a)) == e` from right-id / right-inv / assoc) with recording on
and asserts: the dataset has rows, every feature value is finite, at least
one row is labelled 1, and the proof-set size is `<=` the recorded count.
It also checks the feature schema on a hand-built CP and that recording
stays inert when the flag is off.
