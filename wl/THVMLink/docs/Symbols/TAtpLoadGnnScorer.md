---
Template: Symbol
Name: TAtpLoadGnnScorer
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TAtpLoadGnnScorer
Keywords: [ENIGMA, GNN, GCN, ATP, safetensors, deserialization, load, graph neural network]
SeeAlso: [TAtpSaveGnnScorer, TAtpSetGnnScorer, TAtpGnnScorerAsset, TAtpTrainGnn]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TAtpLoadGnnScorer]()[*path*]</code> loads a GCN scorer `.safetensors` file (saved by [TAtpSaveGnnScorer]()) and returns the `"Model"` Association: the `W1` / `Ws` / `Bh` / `Wout` / `Bout` weights plus `Rounds` / `Hidden` / `NMax`.

## Details & Options

- <code>[TAtpSetGnnScorer]()[*path*]</code> is shorthand for <code>[TAtpSetGnnScorer]()[[TAtpLoadGnnScorer]()[*path*]]</code> - load then push in one call.

## Basic Examples

Load the bundled pretrained scorer and read its config:

```wl
#| eval: false
Needs["WolframInstitute`THVMLink`ATP`"];
model = TAtpLoadGnnScorer[TAtpGnnScorerAsset[]];
{model["Rounds"], model["Hidden"]}
```
<!-- => {<rounds>, <hidden>} - the trained GCN's config -->

## Properties & Relations

- [TAtpSaveGnnScorer]() writes the file this reads; [TAtpGnnScorerAsset]() returns the bundled-asset path.
- [TAtpSetGnnScorer]() pushes the loaded `"Model"` into the C engine; [TAtpGnnScore]() scores graphs with it.

## Possible Issues

- The returned `"Model"` is the same Association [TAtpTrainGnn]() returns under its `"Model"` key, so a file written by some other tool must follow that weight-naming and `__metadata__` layout to reload as a usable scorer.
