---
Template: Symbol
Name: TAtpGnnScore
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TAtpGnnScore
Keywords: [ENIGMA, graph neural network, GNN, theorem proving, scoring, proof relevance, learned guidance]
SeeAlso: [TAtpTrainGnn, TAtpGraphDataset, TAtpCpGraph, TAtpLoadGnnScorer, TFindProofReranked]
RelatedGuides: [THVMLink]
RelatedTutorials: ["[Learned Guidance: a GNN for Critical-Pair Selection](GnnGuidance)"]
---

## Usage

<code>[TAtpGnnScore]()[*model*, *dataset*]</code> scores each graph of *dataset* with the graph neural network *model*, giving a list of scores.

## Details & Options

- *model* is a graph convolutional network (GCN), as [TAtpTrainGnn]() gives in its `"Model"` and [TAtpLoadGnnScorer]() reads from a file.
- *dataset* is an Association with the key `"Graphs"`, a list of [TAtpCpGraph]() graphs, as [TAtpGraphDataset]() gives.
- The score of a graph is the difference between the two outputs of the network, for relevant and not relevant: the higher the score, the more relevant the network considers the equation to a proof.
- The scores are not probabilities, and their scale depends on the model.
- The network passes messages between neighboring nodes for its number of rounds, averages over the nodes, and reads out the two outputs. A model can score graphs of any size.
- [TFindProofReranked]() uses the scores to rank the critical pairs of a proof search.

## Basic Examples

Score two equations with a small network written by hand, with one round and two hidden units:

```wl
TAtpGnnScore[<|"Kind" -> "GNN", "W1" -> {N[IdentityMatrix[{12, 2}]]}, "Ws" -> {N[IdentityMatrix[{12, 2}]]}, "Bh" -> {{0., 0.}}, "Wout" -> {{1., 0.}, {0., 1.}}, "Bout" -> {0., 0.}, "Rounds" -> 1, "Hidden" -> 2, "NMax" -> 64|>, <|"Graphs" -> {TAtpCpGraph[CircleTimes[x, y] == CircleTimes[y, x]], TAtpCpGraph[CircleTimes[x, OverBar[x]] == ident]}|>]
```

<!-- => {-0.68, -0.356667} -->

## Scope

Score the graphs of a dataset:

```wl
TAtpGnnScore[<|"Kind" -> "GNN", "W1" -> {N[IdentityMatrix[{12, 2}]]}, "Ws" -> {N[IdentityMatrix[{12, 2}]]}, "Bh" -> {{0., 0.}}, "Wout" -> {{1., 0.}, {0., 1.}}, "Bout" -> {0., 0.}, "Rounds" -> 1, "Hidden" -> 2, "NMax" -> 64|>, TAtpGraphDataset[{OverBar[OverBar[x]] == x}, AxiomaticTheory["AbelianGroupAxioms"]]]
```

<!-- => {-0.582353, -0.356667, -0.597222, -0.460256, -0.630952, -0.356667, -0.345833, -0.817778, -0.68, -0.555555} -->

---

Score with the bundled pretrained network:

```wl
TAtpGnnScore[TAtpLoadGnnScorer[TAtpGnnScorerAsset[]], <|"Graphs" -> {TAtpCpGraph[CircleTimes[x, y] == CircleTimes[y, x]], TAtpCpGraph[CircleTimes[x, OverBar[x]] == ident]}|>]
```

<!-- => {-5.11179, 0.3535} -->

## Properties and Relations

Equations that differ by a renaming of their symbols have the same score:

```wl
TAtpGnnScore[<|"Kind" -> "GNN", "W1" -> {N[IdentityMatrix[{12, 2}]]}, "Ws" -> {N[IdentityMatrix[{12, 2}]]}, "Bh" -> {{0., 0.}}, "Wout" -> {{1., 0.}, {0., 1.}}, "Bout" -> {0., 0.}, "Rounds" -> 1, "Hidden" -> 2, "NMax" -> 64|>, <|"Graphs" -> {TAtpCpGraph[CircleTimes[x, OverBar[x]] == ident], TAtpCpGraph[p[y, q[y]] == r]}|>]
```

<!-- => {-0.356667, -0.356667} -->
