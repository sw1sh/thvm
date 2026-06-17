---
Template: Symbol
Name: TAtpSaveGnnScorer
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TAtpSaveGnnScorer
Keywords: [ENIGMA, GNN, GCN, ATP, safetensors, serialization, save, paclet asset, graph neural network]
SeeAlso: [TAtpLoadGnnScorer, TAtpSetGnnScorer, TAtpGnnScorerAsset, TAtpTrainGnn]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TAtpSaveGnnScorer]()[*model*, *path*]</code> saves a trained GCN *model* (the `"Model"` Association from [TAtpTrainGnn]()) to *path* as a `.safetensors` file (`TSafeTensorSave`): each weight array becomes one named tensor and the scalar config (`Rounds` / `Hidden` / `NMax`) rides in the file's `__metadata__`. Returns *path*.

## Details & Options

- This is how a pretrained graph convolutional network ships as a paclet asset. The file is the standard safetensors layout, so it is also readable by Python's `safetensors`.
- <code>[TAtpSetGnnScorer]()[*path*]</code> reloads the saved file and pushes it; [TAtpLoadGnnScorer]() reloads it as a `"Model"` Association.

## Basic Examples

Save a trained model to a temporary file:

```wl
#| eval: false
Needs["WolframInstitute`THVMLink`ATP`"];
model = TAtpLoadGnnScorer[TAtpGnnScorerAsset[]];
TAtpSaveGnnScorer[model, FileNameJoin[{$TemporaryDirectory, "gcn.safetensors"}]]
```
<!-- => the saved file path -->

## Properties & Relations

- [TAtpTrainGnn]() produces the `"Model"` this serializes.
- [TAtpLoadGnnScorer]() is the inverse: it reads the file back into a `"Model"` Association. [TAtpGnnScorerAsset]() is the bundled-asset path.

## Possible Issues

- The weights round-trip bit-for-bit through the file, but the scalar config travels in the safetensors `__metadata__`; a hand-edited file that drops `Rounds` / `Hidden` / `NMax` will not reload as a usable scorer.
