---
Template: Symbol
Name: TAtpCpDataset
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TAtpCpDataset
Keywords: [ENIGMA, theorem proving, critical pair, dataset, learned guidance, feature vector, machine learning]
SeeAlso: [TAtpTrainScorer, TAtpSetLearnedScorer, TAtpGraphDataset, TFindProof, AxiomaticTheory]
RelatedGuides: [THVMLink]
RelatedTutorials: ["[Methods and Presets for TFindProof](AtpMethods)"]
---

## Usage

<code>[TAtpCpDataset]()[*conjectures*, *axioms*]</code> proves each of the *conjectures* from *axioms*, and gives the features of the critical pairs each proof processed, labeled by whether they took part in the proof.

<code>[TAtpCpDataset]()["*theory*"]</code> uses the notable theorems of the named [AxiomaticTheory]() theory as the conjectures.

## Details & Options

- [TAtpCpDataset]() gives the training data for a learned critical-pair ranking in the style of ENIGMA (Efficient Learning-Based Inference Guiding Machine): [TAtpTrainScorer]() trains a model on it, [TAtpSetLearnedScorer]() gives the model to the prover, and <code>[TFindProof]()[…, [Method]() -> "ENIGMA"]</code> uses it.
- The result has the keys `"Features"` (a matrix with one row of 14 numbers for each processed critical pair), `"Labels"` (`1` for a critical pair the proof uses and `0` otherwise), `"FeatureNames"`, `"NRows"`, `"NPositive"` (the number of rows labeled `1`) and `"NProofs"` (the number of conjectures that were proved).
- The 14 features describe the size, depth, variables and weights of a critical pair, its age in the search, its top symbols, whether it shares a subterm with the goal, whether it can be oriented, and how it unifies.
- Only the conjectures that are proved contribute rows.
- The following options can be given:

| | | |
| --- | --- | --- |
| [Method]() | `{"Completion"}` | proof search method |
| [TimeConstraint]() | `30` | time limit in seconds for each proof |
| [MaxSteps]() | [Automatic]() | maximum number of critical pairs for each proof |

## Basic Examples

The features of the critical pairs of a one-step proof:

```wl
TAtpCpDataset[{f[f[f[a]]] == f[a]}, {f[f[x_]] == x_}]
```

<!-- => <|"Features" -> {{4, 2, 1, 2, 4, 3, 34, 2000, 0, 3, 0, 1, 1, 4}}, "Labels" -> {1}, "FeatureNames" -> {"size_sum", "max_depth", "n_distinct_vars", "n_var_occ", "weight_add", "weight_gt", "weight_mix2", "goal_weight", "age", "top_symbol_l", "top_symbol_r", "shares_goal_sub", "orientable", "unif_measure"}, "NRows" -> 1, "NPositive" -> 1, "NProofs" -> 1|> -->

## Scope

A proof of the double-inverse theorem of abelian groups processes ten critical pairs, four of which take part in the proof:

```wl
TAtpCpDataset[{OverBar[OverBar[x]] == x}, AxiomaticTheory["AbelianGroupAxioms"]]
```

<!-- => <|"Features" -> {{5, 2, 1, 2, 5, 4, 45, 10000, 0, 3, 0, 0, 1, 4}, ...}, "Labels" -> {0, 1, 1, 0, 0, 0, 0, 0, 1, 1}, "FeatureNames" -> {...}, "NRows" -> 10, "NPositive" -> 4, "NProofs" -> 1|> -->

---

The notable theorems of the group axioms give a larger dataset:

```wl
TAtpCpDataset["GroupAxioms", TimeConstraint -> 5]["NRows"]
```

<!-- => 86 -->

## Options

### Method

Prove with a different critical-pair weight:

```wl
TAtpCpDataset[{OverBar[OverBar[x]] == x}, AxiomaticTheory["AbelianGroupAxioms"], Method -> {"Completion", "CriticalPairWeight" -> "Add"}]["NRows"]
```

<!-- => 9 -->

### TimeConstraint

A conjecture that is not proved within the time limit contributes no rows:

```wl
TAtpCpDataset[{x ** y == y ** x}, AxiomaticTheory["AbelianGroupAxioms"], TimeConstraint -> 1]["NRows"]
```

<!-- => 0 -->

It is not counted as a proof:

```wl
TAtpCpDataset[{x ** y == y ** x}, AxiomaticTheory["AbelianGroupAxioms"], TimeConstraint -> 1]["NProofs"]
```

<!-- => 0 -->

### MaxSteps

A step budget bounds each proof:

```wl
TAtpCpDataset[{OverBar[OverBar[x]] == x}, AxiomaticTheory["AbelianGroupAxioms"], MaxSteps -> 1000]["NRows"]
```

<!-- => 10 -->

## Properties and Relations

[TAtpTrainScorer]() trains a model on the dataset, and [TAtpGraphDataset]() gives a dataset of equation graphs for a graph neural network instead:

```wl
TAtpGraphDataset[{OverBar[OverBar[x]] == x}, AxiomaticTheory["AbelianGroupAxioms"]]["NPos"]
```

<!-- => 5 -->
