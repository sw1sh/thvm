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

Train a scorer on a synthetic dataset of 20 rows, whose label is `1` when the first feature exceeds `0.5`, and read the area under the curve on the training rows:

```wl
SeedRandom[42];
feats = RandomReal[{0, 1}, {20, 14}];
labels = Boole /@ Thread[feats[[All, 1]] > 0.5];
trained = TAtpTrainScorer[<|"Features" -> feats, "Labels" -> labels|>];
trained["TrainAUC"]
```

<!-- => 0.969697 -->

The model has a hidden layer, with standardization folded in, as [TAtpSetLearnedScorer]() takes it:

```wl
Keys @ trained["Model"]
```

<!-- => {"Kind", "Mean", "InvStd", "W1", "B1", "W2", "B2"} -->
