---
Template: Symbol
Name: TAtpGnnScorerAsset
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TAtpGnnScorerAsset
Keywords: [ENIGMA, GNN, GCN, ATP, paclet asset, pretrained, safetensors, graph neural network]
SeeAlso: [TAtpLoadGnnScorer, TAtpSetGnnScorer, TAtpSaveGnnScorer, TAtpTrainGnn]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TAtpGnnScorerAsset]()[]</code> returns the bundled-asset path of the pretrained GCN scorer (`wl/THVMLink/Assets/gcn_atp.safetensors`).

<code>[TAtpSetGnnScorer]()[[TAtpGnnScorerAsset]()[]]</code> loads and pushes it.

## Details & Options

- Returns `Missing["NotBundled"]` if the asset file is absent.

## Basic Examples

Check that the asset is bundled:

```wl
Needs["WolframInstitute`THVMLink`ATP`"];
FileExistsQ @ TAtpGnnScorerAsset[]
```
<!-- => True -->

## Scope

Load and push the bundled scorer:

```wl
#| eval: false
Needs["WolframInstitute`THVMLink`ATP`"];
TAtpSetGnnScorer[TAtpGnnScorerAsset[]]
```
<!-- => True -->

## Properties & Relations

- [TAtpLoadGnnScorer]() reads the asset path into a `"Model"` Association; [TAtpSetGnnScorer]() pushes it into the C engine; [TAtpSaveGnnScorer]() is how such an asset is produced from a [TAtpTrainGnn]() model.

## Possible Issues

- In a checkout or paclet build without the bundled `.safetensors` file, this returns `Missing["NotBundled"]`; train and save a model with [TAtpTrainGnn]() / [TAtpSaveGnnScorer]() instead.
