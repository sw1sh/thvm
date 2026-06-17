---
Template: Symbol
Name: TAtpCpDataset
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TAtpCpDataset
Keywords: [ENIGMA, ATP, critical pair, dataset, learned guidance, feature vector, machine learning, proof relevance]
SeeAlso: [TAtpTrainScorer, TAtpSetLearnedScorer, TAtpGraphDataset, TFindProof, AxiomaticTheory]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TAtpCpDataset]()[*conjectures*, *axioms*]</code> proves each conjecture against the shared *axioms* with per-critical-pair feature recording on, labels the processed critical pairs by trace-DAG reachability from each proof (`1` = proof-relevant, `0` = not), and returns the dataset.

<code>[TAtpCpDataset]()[*theory*]</code> runs every <code>[AxiomaticTheory]()[*theory*, "NotableTheorems"]</code> against the theory's axioms.

The returned Association is keyed `"Features"` (an *n* by 14 matrix), `"Labels"` (a 0/1 vector), `"FeatureNames"`, `"NRows"`, `"NPositive"`, and `"NProofs"`. Only proved runs contribute rows.

## Details & Options

- This is the ENIGMA (Efficient learNing-based Inference Guiding MAchine) training-data foundation for the Tier-1 hand-feature scorer: feed `"Features"` / `"Labels"` to a classifier (for example `TNetTrain`, or directly to [TAtpTrainScorer]()) and push the result back with [TAtpSetLearnedScorer]() to close the learn loop.
- The 14 feature columns are `"size_sum"`, `"max_depth"`, `"n_distinct_vars"`, `"n_var_occ"`, `"weight_add"`, `"weight_gt"`, `"weight_mix2"`, `"goal_weight"`, `"age"`, `"top_symbol_l"`, `"top_symbol_r"`, `"shares_goal_sub"`, `"orientable"`, and `"unif_measure"`.
- The structural-graph counterpart is [TAtpGraphDataset](), which emits symbol-anonymized hypergraphs for a graph neural network rather than these fixed 14-feature rows.
- Options:
  - `Method` - the proof Method (default `{"Completion"}`).
  - `TimeConstraint` - per-proof wall-clock bound (default 30).
  - `MaxSteps` - the saturation step cap (default `Automatic`).

## Basic Examples

Build a labelled critical-pair dataset from a theory's notable theorems:

```wl
#| eval: false
Needs["WolframInstitute`THVMLink`ATP`"];
ds = TAtpCpDataset["AbelianGroupAxioms", TimeConstraint -> 10];
ds["NRows"]
```
<!-- => an integer row count (number of processed critical pairs across proved runs) -->

## Scope

Read the feature names:

```wl
#| eval: false
Needs["WolframInstitute`THVMLink`ATP`"];
TAtpCpDataset["AbelianGroupAxioms", TimeConstraint -> 10]["FeatureNames"]
```
<!-- => {"size_sum", "max_depth", "n_distinct_vars", "n_var_occ", "weight_add", "weight_gt", "weight_mix2", "goal_weight", "age", "top_symbol_l", "top_symbol_r", "shares_goal_sub", "orientable", "unif_measure"} -->

## Properties & Relations

- [TAtpTrainScorer]() consumes the `"Features"` / `"Labels"` keys directly; `TAtpTrainScorer[theory]` and `TAtpTrainScorer[conjectures, axioms]` fold the dataset build and the training into one call.
- [TAtpGraphDataset]() is the graph-structured sibling: per-equation hypergraphs for a GNN instead of per-critical-pair feature rows; it sources clean positives from the verified proof object rather than per-CP trace labels.

## Possible Issues

- Building a dataset from a whole corpus means proving the corpus, so each proof is bounded by `TimeConstraint`; a sweep over `NotableTheorems` can be slow and memory-heavy. Bound and chunk large sweeps.
