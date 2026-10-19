---
Template: Symbol
Name: TAtpSetGnnScorer
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TAtpSetGnnScorer
Keywords: [ENIGMA, graph neural network, GNN, theorem proving, critical pair, clause selection, learned guidance]
SeeAlso: [TAtpTrainGnn, TAtpLoadGnnScorer, TAtpSaveGnnScorer, TAtpGnnScorerAsset, TFindProofGnnReranked, TAtpSetLearnedScorer]
RelatedGuides: [THVMLink]
RelatedTutorials: ["[Learned Guidance: a GNN for Critical-Pair Selection](GnnGuidance)"]
---

## Usage

<code>[TAtpSetGnnScorer]()[*model*]</code> gives the graph neural network *model* to the prover, which then ranks critical pairs with it in a proof search of [TFindProofGnnReranked]().

<code>[TAtpSetGnnScorer]()["*file*"]</code> reads the network from *file* and gives it to the prover.

<code>[TAtpSetGnnScorer]()[[Clear]()]</code> removes the network.

## Details & Options

- [TAtpSetGnnScorer]() gives [True]() when the prover accepts the network, and [False]() otherwise.
- *model* is a graph convolutional network (GCN), as [TAtpTrainGnn]() gives in its `"Model"` and [TAtpLoadGnnScorer]() reads from a file. Its first layer takes the 12 node features of [TAtpCpGraph]().
- The prover runs the network itself, with no evaluation in the Wolfram Language during the proof search.
- <code>[TAtpSetGnnScorer]()[[None]()]</code> is equivalent to <code>[TAtpSetGnnScorer]()[[Clear]()]</code>.
- A *model* without the keys `"W1"`, `"Ws"`, `"Bh"`, `"Wout"` and `"Bout"` is refused with the message `TAtpSetGnnScorer::model`, and a network whose first layer does not take 12 features is rejected with the message `TAtpSetGnnScorer::rejected`; both give [False]().
- [TAtpSetLearnedScorer]() gives the prover a model of critical-pair features instead.

## Basic Examples

Give the prover a small network written by hand, with one round and two hidden units:

```wl
TAtpSetGnnScorer[<|"Kind" -> "GNN", "W1" -> {N[IdentityMatrix[{12, 2}]]}, "Ws" -> {N[IdentityMatrix[{12, 2}]]}, "Bh" -> {{0., 0.}}, "Wout" -> {{1., 0.}, {0., 1.}}, "Bout" -> {0., 0.}, "Rounds" -> 1, "Hidden" -> 2, "NMax" -> 64|>]
```

<!-- => True -->

## Scope

Give the prover the bundled pretrained network from its file:

```wl
TAtpSetGnnScorer[TAtpGnnScorerAsset[]]
```

<!-- => True -->

---

Remove the network:

```wl
TAtpSetGnnScorer[Clear]
```

<!-- => True -->

---

[None]() removes the network too:

```wl
TAtpSetGnnScorer[None]
```

<!-- => True -->

## Properties and Relations

[TFindProofGnnReranked]() gives a network to the prover and ranks the critical pairs of a proof search with it:

```wl
TFindProofGnnReranked[inv[inv[a]] == a, {ForAll[{x, y, z}, CircleTimes[CircleTimes[x, y], z] == CircleTimes[x, CircleTimes[y, z]]], ForAll[x, CircleTimes[x, ident] == x], ForAll[x, CircleTimes[x, inv[x]] == ident]}, TAtpLoadGnnScorer[TAtpGnnScorerAsset[]]]
```

<!-- => "PROVED" -->

## Possible Issues

A model without the weights of a GCN is refused:

```wl
TAtpSetGnnScorer[<|"Kind" -> "GNN", "W1" -> {1, 2}|>]
```

<!-- => the message TAtpSetGnnScorer::model is issued and the result is False -->

---

A network whose first layer takes 6 features instead of 12 is rejected:

```wl
TAtpSetGnnScorer[<|"Kind" -> "GNN", "W1" -> {N[IdentityMatrix[{6, 2}]]}, "Ws" -> {N[IdentityMatrix[{6, 2}]]}, "Bh" -> {{0., 0.}}, "Wout" -> {{1., 0.}, {0., 1.}}, "Bout" -> {0., 0.}, "Rounds" -> 1, "Hidden" -> 2, "NMax" -> 64|>]
```

<!-- => the message TAtpSetGnnScorer::rejected is issued and the result is False -->
