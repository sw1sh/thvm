---
Template: Symbol
Name: TAtpGnnScorerAsset
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TAtpGnnScorerAsset
Keywords: [ENIGMA, graph neural network, GNN, theorem proving, pretrained model, safetensors, paclet asset]
SeeAlso: [TAtpLoadGnnScorer, TAtpSetGnnScorer, TAtpSaveGnnScorer, TAtpTrainGnn, TAtpGnnScore]
RelatedGuides: [THVMLink]
RelatedTutorials: ["[Learned Guidance: a GNN for Critical-Pair Selection](GnnGuidance)"]
---

## Usage

<code>[TAtpGnnScorerAsset]()[]</code> gives the path of the pretrained graph neural network for ranking critical pairs that comes with THVMLink.

## Details & Options

- The file `gcn_atp.safetensors` is in the `Assets` directory of the paclet, in the safetensors format.
- The network is a graph convolutional network (GCN) with three rounds of message passing and 64 hidden units, trained on the critical-pair graphs of proof searches of unit-equality problems.
- [TAtpLoadGnnScorer]() reads the network as a model, and [TAtpSetGnnScorer]() gives it to the prover.
- When the file is not present, [TAtpGnnScorerAsset]() gives <code>[Missing]()["NotBundled"]</code>.

## Basic Examples

The path of the pretrained network:

```wl
TAtpGnnScorerAsset[]
```

<!-- => the path of gcn_atp.safetensors in the Assets directory of the paclet -->

## Scope

The name of the file:

```wl
FileNameTake[TAtpGnnScorerAsset[]]
```

<!-- => "gcn_atp.safetensors" -->

---

Give the pretrained network to the prover:

```wl
TAtpSetGnnScorer[TAtpGnnScorerAsset[]]
```

<!-- => True -->

## Properties and Relations

[TAtpLoadGnnScorer]() reads the network; it has three rounds of message passing:

```wl
TAtpLoadGnnScorer[TAtpGnnScorerAsset[]]["Rounds"]
```

<!-- => 3 -->

The number of hidden units:

```wl
TAtpLoadGnnScorer[TAtpGnnScorerAsset[]]["Hidden"]
```

<!-- => 64 -->
