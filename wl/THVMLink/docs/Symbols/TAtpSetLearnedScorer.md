---
Template: Symbol
Name: TAtpSetLearnedScorer
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TAtpSetLearnedScorer
Keywords: [ENIGMA, ATP, critical pair, learned scorer, clause selection, logistic regression, MLP, proof relevance]
SeeAlso: [TAtpTrainScorer, TAtpCpDataset, TAtpSetGnnScorer, TFindProof]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TAtpSetLearnedScorer]()[*model*]</code> pushes a trained critical-pair selection model into the C ATP engine; proofs run with `"CriticalPairWeight"` set to `"Learned"` (equivalently `Method -> "ENIGMA"`) then use it instead of the baked-in logistic regression. Returns `True` on success, `False` on a malformed model.

<code>[TAtpSetLearnedScorer]()[Clear]</code> (or `None`) drops the model and reverts to the baked-in scorer.

## Details & Options

- *model* is an Association keyed `"Kind"` (`"Linear"` or `"MLP"`), `"Mean"`, `"InvStd"`, and the weights (`"W"` / `"B"` for linear, `"W1"` / `"B1"` / `"W2"` / `"B2"` for a one-hidden-layer ReLU network, hidden width at most 64). It is exactly the `"Model"` Association [TAtpTrainScorer]() returns.
- Features are standardized by `"Mean"` and `"InvStd"` before the forward pass (default identity); the model outputs a raw logit, higher meaning selected sooner.
- This is the Tier-1 hand-feature scorer push. The Tier-2 graph-neural-network analogue is [TAtpSetGnnScorer]().

## Basic Examples

Push a linear model written out by hand (the same shape [TAtpTrainScorer]() returns as its `"Model"`) - the negative weight on the first feature, `size_sum`, ranks small critical pairs sooner:

```wl
TAtpSetLearnedScorer[<|
    "Kind" -> "Linear",
    "Mean" -> ConstantArray[0., 14],
    "InvStd" -> ConstantArray[1., 14],
    "W" -> N @ -UnitVector[14, 1],
    "B" -> 0.|>]
```
<!-- => True -->

## Scope

Drop the learned model and revert to the baked-in scorer:

```wl
TAtpSetLearnedScorer[Clear]
```
<!-- => True -->

## Properties & Relations

- [TAtpTrainScorer]() produces the `"Model"` this consumes; [TAtpCpDataset]() produces the dataset that trains it.
- After the push, <code>[TFindProof]()[*goal*, *theory*, Method -> "ENIGMA"]</code> ranks the critical-pair queue by the learned model. Completeness holds regardless because the engine still takes a periodic first-in-first-out pick.
- [TAtpSetGnnScorer]() is the graph-neural-network counterpart.

## Possible Issues

- A malformed model returns `False` (no exception); check the return before assuming the scorer was installed.
