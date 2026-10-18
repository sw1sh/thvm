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

- The bundled scorer has three rounds of message passing, 64 hidden units and the 12 clause-graph node features the engine computes; it is the ENIGMA Tier-2 GCN (DAgger round 3), trained on critical-pair graphs from the proof searches of CASC-29 and CASC-30 UEQ problems.
- Returns `Missing["NotBundled"]` if the asset file is absent.

## Basic Examples

Check that the asset is bundled:

```wl
FileExistsQ @ TAtpGnnScorerAsset[]
```
<!-- => True -->

## Scope

Load and push the bundled scorer:

```wl
TAtpSetGnnScorer[TAtpGnnScorerAsset[]]
```
<!-- => True -->

## Properties & Relations

- [TAtpLoadGnnScorer]() reads the asset path into a `"Model"` Association; [TAtpSetGnnScorer]() pushes it into the C engine; [TAtpSaveGnnScorer]() is how such an asset is produced from a [TAtpTrainGnn]() model.

## Possible Issues

- In a checkout or paclet build without the bundled `.safetensors` file, this returns `Missing["NotBundled"]`; train and save a model with [TAtpTrainGnn]() / [TAtpSaveGnnScorer]() instead.
