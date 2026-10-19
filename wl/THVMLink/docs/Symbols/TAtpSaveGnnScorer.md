---
Template: Symbol
Name: TAtpSaveGnnScorer
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TAtpSaveGnnScorer
Keywords: [ENIGMA, graph neural network, GNN, theorem proving, safetensors, save, model file]
SeeAlso: [TAtpLoadGnnScorer, TAtpSetGnnScorer, TAtpGnnScorerAsset, TAtpTrainGnn]
RelatedGuides: [THVMLink]
RelatedTutorials: ["[Learned Guidance: a GNN for Critical-Pair Selection](GnnGuidance)"]
---

## Usage

<code>[TAtpSaveGnnScorer]()[*model*, "*file*"]</code> saves the graph neural network *model* to *file* in the safetensors format, and gives *file*.

## Details & Options

- *model* is a graph convolutional network (GCN), as [TAtpTrainGnn]() gives in its `"Model"`, with the keys `"Kind"`, `"W1"`, `"Ws"`, `"Bh"`, `"Wout"`, `"Bout"`, `"Rounds"`, `"Hidden"` and `"NMax"`.
- Each weight array is saved as a named tensor, and `"Rounds"`, `"Hidden"` and `"NMax"` are saved in the metadata of the file.
- The safetensors format can be read by other programs, such as the `safetensors` library for Python.
- [TAtpLoadGnnScorer]() reads the file back as a model, and <code>[TAtpSetGnnScorer]()["*file*"]</code> gives it to the prover.

## Basic Examples

Save a small network written by hand, with one round and two hidden units:

```wl
TAtpSaveGnnScorer[<|"Kind" -> "GNN", "W1" -> {N[IdentityMatrix[{12, 2}]]}, "Ws" -> {N[IdentityMatrix[{12, 2}]]}, "Bh" -> {{0., 0.}}, "Wout" -> {{1., 0.}, {0., 1.}}, "Bout" -> {0., 0.}, "Rounds" -> 1, "Hidden" -> 2, "NMax" -> 64|>, FileNameJoin[{$TemporaryDirectory, "gcn.safetensors"}]]
```

<!-- => the path of gcn.safetensors in $TemporaryDirectory -->

## Scope

Save the bundled pretrained network to another file:

```wl
TAtpSaveGnnScorer[TAtpLoadGnnScorer[TAtpGnnScorerAsset[]], FileNameJoin[{$TemporaryDirectory, "pretrained.safetensors"}]]
```

<!-- => the path of pretrained.safetensors in $TemporaryDirectory -->

## Properties and Relations

A small network written by hand, with one round and two hidden units:

```wl
model = <|"Kind" -> "GNN", "W1" -> {N[IdentityMatrix[{12, 2}]]}, "Ws" -> {N[IdentityMatrix[{12, 2}]]}, "Bh" -> {{0., 0.}}, "Wout" -> {{1., 0.}, {0., 1.}}, "Bout" -> {0., 0.}, "Rounds" -> 1, "Hidden" -> 2, "NMax" -> 64|>
```

<!-- => the model Association -->

Loading the saved file gives the model back:

```wl
TAtpLoadGnnScorer[TAtpSaveGnnScorer[model, FileNameJoin[{$TemporaryDirectory, "gcn.safetensors"}]]] === model
```

<!-- => True -->

---

The saved file can be given to the prover:

```wl
TAtpSetGnnScorer[TAtpSaveGnnScorer[<|"Kind" -> "GNN", "W1" -> {N[IdentityMatrix[{12, 2}]]}, "Ws" -> {N[IdentityMatrix[{12, 2}]]}, "Bh" -> {{0., 0.}}, "Wout" -> {{1., 0.}, {0., 1.}}, "Bout" -> {0., 0.}, "Rounds" -> 1, "Hidden" -> 2, "NMax" -> 64|>, FileNameJoin[{$TemporaryDirectory, "gcn.safetensors"}]]]
```

<!-- => True -->
