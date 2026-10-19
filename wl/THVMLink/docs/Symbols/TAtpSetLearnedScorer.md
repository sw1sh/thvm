---
Template: Symbol
Name: TAtpSetLearnedScorer
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TAtpSetLearnedScorer
Keywords: [ENIGMA, theorem proving, critical pair, learned scorer, clause selection, logistic regression, neural network]
SeeAlso: [TAtpTrainScorer, TAtpCpDataset, TAtpSetGnnScorer, TFindProof]
RelatedGuides: [THVMLink]
RelatedTutorials: ["[Methods and Presets for TFindProof](AtpMethods)"]
---

## Usage

<code>[TAtpSetLearnedScorer]()[*model*]</code> makes the prover rank critical pairs with the learned *model* when the critical-pair weight is `"Learned"`.

<code>[TAtpSetLearnedScorer]()[[Clear]()]</code> removes the model, so that the prover uses its built-in learned ranking.

## Details & Options

- [TAtpSetLearnedScorer]() gives [True]() when the prover accepts the model, and [False]() otherwise.
- The method <code>[TFindProof]()[…, [Method]() -> "ENIGMA"]</code>, or any configuration with `"CriticalPairWeight" -> "Learned"`, ranks critical pairs with the model. The prover still selects a pair in order of age from time to time, so every proof the prover can find is still found.
- *model* is an Association with the keys `"Kind"` (`"Linear"` or `"MLP"`), `"Mean"` and `"InvStd"` (with which each of the 14 features of a critical pair is standardized), and the weights: `"W"` and `"B"` for a linear model, or `"W1"`, `"B1"`, `"W2"` and `"B2"` for a network with one hidden layer of at most 64 units. It is the `"Model"` of [TAtpTrainScorer]().
- The model gives one number for each critical pair, and the prover selects the pairs with higher numbers sooner.
- <code>[TAtpSetLearnedScorer]()[[None]()]</code> is equivalent to <code>[TAtpSetLearnedScorer]()[[Clear]()]</code>.
- [TAtpSetGnnScorer]() gives the prover a graph neural network instead.

## Basic Examples

Give the prover a linear model whose negative weight on the first feature, the size of a critical pair, selects small pairs sooner:

```wl
TAtpSetLearnedScorer[<|"Kind" -> "Linear", "Mean" -> ConstantArray[0., 14], "InvStd" -> ConstantArray[1., 14], "W" -> N[-UnitVector[14, 1]], "B" -> 0.|>]
```

<!-- => True -->

Prove a theorem with the learned ranking:

```wl
TFindProof[OverBar[OverBar[x]] == x, "AbelianGroupAxioms", Method -> "ENIGMA"]
```

<!-- => a ProofObject summary box, logic EquationalLogic -->

Remove the model:

```wl
TAtpSetLearnedScorer[Clear]
```

<!-- => True -->

## Scope

[None]() removes the model too:

```wl
TAtpSetLearnedScorer[None]
```

<!-- => True -->

---

The critical pairs of a proof:

```wl
dataset = TAtpCpDataset[{OverBar[OverBar[x]] == x}, AxiomaticTheory["AbelianGroupAxioms"]]
```

<!-- => <|"Features" -> {...10 rows...}, "Labels" -> {0, 1, 1, 0, 0, 0, 0, 0, 1, 1}, ..., "NRows" -> 10, "NPositive" -> 4, "NProofs" -> 1|> -->

A linear model trained on them:

```wl
trained = TAtpTrainScorer[dataset, "Hidden" -> 0]
```

<!-- => <|"Model" -> <|"Kind" -> "Linear", "Mean" -> {...}, "InvStd" -> {...}, "W" -> {...}, "B" -> ...|>, "TrainAUC" -> 0.708333, "NRows" -> 10, "NPositive" -> 4, "Hidden" -> 0|> -->

Give the model to the prover:

```wl
TAtpSetLearnedScorer[trained["Model"]]
```

<!-- => True -->

## Properties and Relations

The `"ENIGMA"` configuration ranks critical pairs with the learned model:

```wl
TAtpDescribeMethod["ENIGMA"]
```

<!-- => <|"CriticalPairWeight" -> "Learned", "Ordering" -> "KBO", "AutoPrecedence" -> True, "UnfailingCP" -> True, "RHSInterreduce" -> True, "AutoMaxWeight" -> 20, "CoopWeight" -> "Gt", "CoopRatio" -> 2|> -->

## Possible Issues

A model without its weights is rejected, with a message:

```wl
TAtpSetLearnedScorer[<|"Kind" -> "Linear", "W" -> {1, 2}|>]
```

<!-- => the message LibraryFunction::cfta is issued and the result is False -->
