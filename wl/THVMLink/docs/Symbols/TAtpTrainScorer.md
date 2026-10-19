---
Template: Symbol
Name: TAtpTrainScorer
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TAtpTrainScorer
Keywords: [ENIGMA, theorem proving, critical pair, learned guidance, training, logistic regression, neural network]
SeeAlso: [TAtpCpDataset, TAtpSetLearnedScorer, TAtpTrainGnn, TFindProof]
RelatedGuides: [THVMLink]
RelatedTutorials: ["[Methods and Presets for TFindProof](AtpMethods)"]
---

## Usage

<code>[TAtpTrainScorer]()[*dataset*]</code> trains a model that ranks critical pairs by their relevance to a proof, on a *dataset* of critical-pair features from [TAtpCpDataset]().

<code>[TAtpTrainScorer]()["*theory*"]</code> trains on the dataset of the notable theorems of the named [AxiomaticTheory]() theory.

<code>[TAtpTrainScorer]()[*conjectures*, *axioms*]</code> trains on the dataset of the proofs of *conjectures* from *axioms*.

## Details & Options

- *dataset* is an Association with the keys `"Features"`, a matrix with one row of 14 features for each critical pair, and `"Labels"`, `1` for a critical pair that takes part in a proof and `0` otherwise, as [TAtpCpDataset]() gives.
- [TAtpTrainScorer]() trains a network with THVMLink's own tensor runtime and gives an Association with the keys `"Model"`, `"TrainAUC"`, `"NRows"`, `"NPositive"` and `"Hidden"`, and `"NProofs"` when it builds the dataset itself.
- `"Model"` is the Association that [TAtpSetLearnedScorer]() takes. It holds the mean and inverse standard deviation of each feature, with which the features are standardized, and the weights of the network, which gives one number for each critical pair: the higher the number, the sooner the prover selects the pair.
- With `"Hidden" -> 0` the model is linear, with the kind `"Linear"` and the weights `"W"` and `"B"`. With `"Hidden" -> n`, it has one hidden layer of *n* rectified linear units, at most 64, with the kind `"MLP"` and the weights `"W1"`, `"B1"`, `"W2"` and `"B2"`.
- `"TrainAUC"` is the area under the receiver operating characteristic (ROC) curve of the model on the training rows: `1` when the model ranks every label-1 row above every label-0 row, and about `0.5` for a random ranking.
- With the forms that build the dataset, the options of [TAtpCpDataset]() are used for the proofs.
- A *dataset* whose rows are not 14 numbers each, or whose labels are not one `0` or `1` for each row, issues `TAtpTrainScorer::data` and gives `$Failed`; a `"Hidden"` value outside `0` to `64` issues `TAtpTrainScorer::hidden`.
- The model is used by <code>[TFindProof]()[…, [Method]() -> "ENIGMA"]</code> after <code>[TAtpSetLearnedScorer]()[*model*]</code>, in the style of ENIGMA (Efficient Learning-Based Inference Guiding Machine).
- [TAtpTrainGnn]() trains a graph neural network on equation graphs instead of feature rows.
- The following options can be given:

| | | |
| --- | --- | --- |
| `"Hidden"` | `16` | number of hidden units, or `0` for a linear model |
| [MaxTrainingRounds]() | `300` | number of training steps |
| `"LearningRate"` | `0.01` | learning rate of the optimizer |
| `"Method"` | `"Adam"` | optimizer |

## Basic Examples

A dataset of six critical pairs, each a row of 14 features, labeled by whether it takes part in a proof:

```wl
data = <|"Features" -> Table[N[Mod[i + j, 4]], {i, 6}, {j, 14}], "Labels" -> {1, 0, 0, 0, 1, 0}|>
```

<!-- => <|"Features" -> {{2., 3., 0., 1., 2., 3., 0., 1., 2., 3., 0., 1., 2., 3.}, ...}, "Labels" -> {1, 0, 0, 0, 1, 0}|> -->

Train a linear scorer on it:

```wl
TAtpTrainScorer[data, "Hidden" -> 0]
```

<!-- => <|"Model" -> <|"Kind" -> "Linear", "Mean" -> {1.83333, 1.5, ...}, "InvStd" -> {0.855399, 0.725476, ...}, "W" -> {...}, "B" -> a number|>, "TrainAUC" -> 1., "NRows" -> 6, "NPositive" -> 2, "Hidden" -> 0|> -->

The area under the curve on the training rows:

```wl
TAtpTrainScorer[data, "Hidden" -> 0]["TrainAUC"]
```

<!-- => 1. -->

## Scope

By default the model has one hidden layer of 16 units:

```wl
Keys[TAtpTrainScorer[<|"Features" -> Table[N[Mod[i + j, 4]], {i, 6}, {j, 14}], "Labels" -> {1, 0, 0, 0, 1, 0}|>]["Model"]]
```

<!-- => {"Kind", "Mean", "InvStd", "W1", "B1", "W2", "B2"} -->

---

Train on the critical pairs of the proofs of conjectures, here from the axioms of an Abelian group:

```wl
KeyDrop[TAtpTrainScorer[{OverBar[OverBar[x]] == x}, AxiomaticTheory["AbelianGroupAxioms"], TimeConstraint -> 5], "Model"]
```

<!-- => <|"TrainAUC" -> 0.958333, "NRows" -> 10, "NPositive" -> 4, "Hidden" -> 16, "NProofs" -> 1|> -->

---

Train on the notable theorems of a named theory:

```wl
KeyDrop[TAtpTrainScorer["AbelianGroupAxioms", TimeConstraint -> 5], "Model"]
```

<!-- => <|"TrainAUC" -> 0.669235, "NRows" -> 92, "NPositive" -> 33, "Hidden" -> 16, "NProofs" -> 4|> -->

## Options

### Hidden

The number of hidden units sets the shape of the first weight matrix:

```wl
Dimensions[TAtpTrainScorer[<|"Features" -> Table[N[Mod[i + j, 4]], {i, 6}, {j, 14}], "Labels" -> {1, 0, 0, 0, 1, 0}|>, "Hidden" -> 4]["Model"]["W1"]]
```

<!-- => {4, 14} -->

### MaxTrainingRounds

A single training step ranks the rows of a real dataset less well than the default 300:

```wl
TAtpTrainScorer[{OverBar[OverBar[x]] == x}, AxiomaticTheory["AbelianGroupAxioms"], TimeConstraint -> 5, MaxTrainingRounds -> 1]["TrainAUC"]
```

<!-- => a number below 0.958333, such as 0.333333 -->

### LearningRate

A larger learning rate:

```wl
TAtpTrainScorer[<|"Features" -> Table[N[Mod[i + j, 4]], {i, 6}, {j, 14}], "Labels" -> {1, 0, 0, 0, 1, 0}|>, "LearningRate" -> 0.1]["TrainAUC"]
```

<!-- => 1. -->

### Method

Train with plain stochastic gradient descent instead of Adam:

```wl
TAtpTrainScorer[<|"Features" -> Table[N[Mod[i + j, 4]], {i, 6}, {j, 14}], "Labels" -> {1, 0, 0, 0, 1, 0}|>, "Method" -> "SGD"]["TrainAUC"]
```

<!-- => 1. -->

## Applications

Train a scorer on the proofs of a theory's notable theorems:

```wl
trained = TAtpTrainScorer["AbelianGroupAxioms", TimeConstraint -> 5];
```

Give its model to the prover:

```wl
TAtpSetLearnedScorer[trained["Model"]]
```

<!-- => True -->

Prove a theorem with the learned scorer:

```wl
TFindProof["InverseOfInverse", "AbelianGroupAxioms", "Status", Method -> "ENIGMA", TimeConstraint -> 10]
```

<!-- => "Proved" -->

## Properties and Relations

The model takes the 14 features of [TAtpCpDataset](), whose names its `"FeatureNames"` key lists:

```wl
Length[TAtpCpDataset[{OverBar[OverBar[x]] == x}, AxiomaticTheory["AbelianGroupAxioms"], TimeConstraint -> 5]["FeatureNames"]]
```

<!-- => 14 -->

## Possible Issues

A dataset whose rows do not have 14 features issues a message and gives `$Failed`:

```wl
TAtpTrainScorer[<|"Features" -> {{1., 2.}}, "Labels" -> {1}|>]
```

<!-- => TAtpTrainScorer::data is issued; $Failed -->
