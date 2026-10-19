---
Template: Symbol
Name: TAtpLoadGnnScorer
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TAtpLoadGnnScorer
Keywords: [ENIGMA, graph neural network, GNN, theorem proving, safetensors, load, model file]
SeeAlso: [TAtpSaveGnnScorer, TAtpSetGnnScorer, TAtpGnnScorerAsset, TAtpTrainGnn, TAtpGnnScore]
RelatedGuides: [THVMLink]
RelatedTutorials: ["[Learned Guidance: a GNN for Critical-Pair Selection](GnnGuidance)"]
---

## Usage

<code>[TAtpLoadGnnScorer]()["*file*"]</code> reads a graph neural network from *file*, saved by [TAtpSaveGnnScorer](), and gives it as a model.

## Details & Options

- The model is an Association with the keys `"Kind"` (`"GNN"`), the weights `"W1"` and `"Ws"` (for each round, the weights applied to the average of the neighbors of a node and to the node itself), `"Bh"` (the biases of each round), `"Wout"` and `"Bout"` (the readout to the two outputs), and `"Rounds"`, `"Hidden"` and `"NMax"`.
- The model is the `"Model"` that [TAtpTrainGnn]() gives, and the form that [TAtpGnnScore]() and [TAtpSetGnnScorer]() take.
- <code>[TAtpSetGnnScorer]()["*file*"]</code> is equivalent to <code>[TAtpSetGnnScorer]()[[TAtpLoadGnnScorer]()["*file*"]]</code>.
- A file that does not exist gives `$Failed`.

## Basic Examples

Save a small network written by hand, with one round and two hidden units:

```wl
file = TAtpSaveGnnScorer[<|"Kind" -> "GNN", "W1" -> {N[IdentityMatrix[{12, 2}]]}, "Ws" -> {N[IdentityMatrix[{12, 2}]]}, "Bh" -> {{0., 0.}}, "Wout" -> {{1., 0.}, {0., 1.}}, "Bout" -> {0., 0.}, "Rounds" -> 1, "Hidden" -> 2, "NMax" -> 64|>, FileNameJoin[{$TemporaryDirectory, "gcn.safetensors"}]]
```

<!-- => the path of gcn.safetensors in $TemporaryDirectory -->

Read it back:

```wl
TAtpLoadGnnScorer[file]
```

<!-- => <|"Kind" -> "GNN", "W1" -> {{{1., 0.}, {0., 1.}, {0., 0.}, ...}}, "Ws" -> {...}, "Bh" -> {{0., 0.}}, "Wout" -> {{1., 0.}, {0., 1.}}, "Bout" -> {0., 0.}, "Rounds" -> 1, "Hidden" -> 2, "NMax" -> 64|> -->

## Scope

The bundled pretrained network has three rounds:

```wl
TAtpLoadGnnScorer[TAtpGnnScorerAsset[]]["Rounds"]
```

<!-- => 3 -->

---

The dimensions of its weights for each round:

```wl
Dimensions /@ TAtpLoadGnnScorer[TAtpGnnScorerAsset[]]["W1"]
```

<!-- => {{12, 64}, {64, 64}, {64, 64}} -->

## Properties and Relations

[TAtpGnnScore]() scores graphs with the model:

```wl
TAtpGnnScore[TAtpLoadGnnScorer[TAtpGnnScorerAsset[]], <|"Graphs" -> {TAtpCpGraph[CircleTimes[x, OverBar[x]] == ident]}|>]
```

<!-- => {0.3535} -->

## Possible Issues

A file that does not exist gives `$Failed`:

```wl
TAtpLoadGnnScorer["/nonexistent/gcn.safetensors"]
```

<!-- => $Failed -->
