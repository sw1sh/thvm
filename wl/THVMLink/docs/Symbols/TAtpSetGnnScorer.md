---
Template: Symbol
Name: TAtpSetGnnScorer
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TAtpSetGnnScorer
Keywords: [ENIGMA, GNN, GCN, ATP, scorer, clause selection, safetensors, graph neural network, learned guidance]
SeeAlso: [TAtpTrainGnn, TAtpLoadGnnScorer, TAtpSaveGnnScorer, TAtpGnnScorerAsset, TFindProofGnnReranked, TAtpSetLearnedScorer]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TAtpSetGnnScorer]()[*model*]</code> pushes a trained GCN *model* (the `"Model"` Association from [TAtpTrainGnn]()) into the C ATP engine, so a persistent proof handle with a non-zero re-rank period ([TFindProofGnnReranked]()) re-ranks the critical-pair queue by running the graph convolutional network forward on `thvm`'s own tensor runtime in C, with no Wolfram round-trip in the proof loop.

<code>[TAtpSetGnnScorer]()[*path*]</code> loads a pretrained GCN from a `.safetensors` file (`TSafeTensorLoad`, lazy mmap-backed) and pushes it.

<code>[TAtpSetGnnScorer]()[Clear]</code> (or `None`) drops the model.

Returns `True` on success, `False` on a malformed model.

## Details & Options

- This is the Tier-2 graph-neural-network scorer push; the Tier-1 hand-feature analogue is [TAtpSetLearnedScorer]().
- <code>[TAtpSetGnnScorer]()[*path*]</code> is shorthand for <code>[TAtpSetGnnScorer]()[[TAtpLoadGnnScorer]()[*path*]]</code>.

## Basic Examples

Push the bundled pretrained scorer:

```wl
#| eval: false
Needs["WolframInstitute`THVMLink`ATP`"];
TAtpSetGnnScorer[TAtpGnnScorerAsset[]]
```
<!-- => True -->

## Scope

Drop the scorer and revert:

```wl
Needs["WolframInstitute`THVMLink`ATP`"];
TAtpSetGnnScorer[Clear]
```
<!-- => True -->

## Properties & Relations

- [TAtpTrainGnn]() produces the `"Model"`; [TAtpLoadGnnScorer]() / [TAtpGnnScorerAsset]() supply a file path; [TAtpSaveGnnScorer]() writes one.
- [TFindProofGnnReranked]() pushes the model itself, then drives one C-side re-ranked saturation.
- [TAtpSetLearnedScorer]() is the feature-vector-scorer counterpart.

## Possible Issues

- A malformed model returns `False` (no exception); check the return before assuming the scorer was installed.
