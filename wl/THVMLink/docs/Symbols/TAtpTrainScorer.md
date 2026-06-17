---
Template: Symbol
Name: TAtpTrainScorer
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TAtpTrainScorer
Keywords: [ENIGMA, ATP, critical pair, scorer, training, learned guidance, logistic regression, MLP, proof relevance]
SeeAlso: [TAtpCpDataset, TAtpSetLearnedScorer, TAtpTrainGnn, TFindProof]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TAtpTrainScorer]()[*dataset*]</code> trains a critical-pair selection model on a [TAtpCpDataset]() result (or any Association keyed `"Features"` and `"Labels"`) with `thvm`'s own `TNetTrain` and returns `<|"Model", "TrainAUC", "NRows", "NPositive", "Hidden"|>`.

<code>[TAtpTrainScorer]()[*theory*]</code> and <code>[TAtpTrainScorer]()[*conjectures*, *axioms*]</code> prepare the dataset via [TAtpCpDataset]() and train in one call (the result also reports `"NProofs"`).

The `"Model"` is exactly the Association [TAtpSetLearnedScorer]() consumes (the Mean / InvStd standardization folded in).

## Details & Options

- A two-class softmax network is trained and its head collapsed to the single proof-relevance logit the engine ranks by. The reported `"TrainAUC"` is the rank area-under-the-ROC-curve on the training rows.
- Pair with [TAtpSetLearnedScorer]() to close the ENIGMA (Efficient learNing-based Inference Guiding MAchine) loop: prove, dataset, train, push, then re-prove with <code>[TFindProof]()[..., Method -> "ENIGMA"]</code>.
- The graph-structured counterpart is [TAtpTrainGnn](), which trains a graph convolutional network on [TAtpGraphDataset]() output instead of these fixed feature vectors.
- Options:
  - `"Hidden"` - `0` for a linear / logistic model, `> 0` for a one-hidden-layer ReLU multilayer perceptron (default 16, max 64).
  - `MaxTrainingRounds` - the optimizer round cap.
  - `"LearningRate"` - the Adam step size.
  - `"Method"` - the optimizer.
  - When called on a theory or a `(conjectures, axioms)` pair, the dataset options route to the proof phase and the rest to training.

## Basic Examples

Train a scorer in one call from a theory's notable theorems:

```wl
#| eval: false
Needs["WolframInstitute`THVMLink`ATP`"];
trained = TAtpTrainScorer["AbelianGroupAxioms", TimeConstraint -> 10];
trained["TrainAUC"]
```
<!-- => a Real in [0, 1] (training-set rank AUC) -->

## Scope

Train on a pre-built dataset:

```wl
#| eval: false
Needs["WolframInstitute`THVMLink`ATP`"];
ds = TAtpCpDataset["AbelianGroupAxioms", TimeConstraint -> 10];
TAtpTrainScorer[ds, "Hidden" -> 16]["Model"]
```
<!-- => <|"Kind" -> "MLP", "Mean" -> {...}, "InvStd" -> {...}, "W1" -> {...}, ...|> -->

## Properties & Relations

- [TAtpCpDataset]() produces the `"Features"` / `"Labels"` this consumes.
- [TAtpSetLearnedScorer]() pushes the resulting `"Model"` into the C engine; then <code>[TFindProof]()[..., Method -> "ENIGMA"]</code> ranks critical pairs by it.
- [TAtpTrainGnn]() is the Tier-2 graph-neural-network analogue.

## Possible Issues

- A `(theory)` or `(conjectures, axioms)` call proves the whole corpus first; bound it with the dataset's `TimeConstraint` and watch memory on large sweeps.
