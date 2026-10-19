---
Template: Symbol
Name: TFindProofReranked
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TFindProofReranked
Keywords: [ENIGMA, graph neural network, GNN, theorem proving, critical pair, ranking, learned guidance]
SeeAlso: [TFindProofGnnReranked, TAtpGnnScore, TAtpSetGnnScorer, TAtpTrainGnn, TAtpLoadGnnScorer, TFindProof]
RelatedGuides: [THVMLink]
RelatedTutorials: ["[Learned Guidance: a GNN for Critical-Pair Selection](GnnGuidance)"]
---

## Usage

<code>[TFindProofReranked]()[*conjecture*, *axioms*, *model*]</code> proves *conjecture* from *axioms*, ranking the queue of critical pairs with the graph neural network *model* at regular intervals, and gives the status of the proof search.

## Details & Options

- *model* is a graph convolutional network (GCN), as [TAtpTrainGnn]() gives in its `"Model"` and [TAtpLoadGnnScorer]() reads from a file.
- [TFindProofReranked]() runs the proof search in chunks of `"RerankPeriod"` steps. After each chunk it encodes the queued critical pairs with [TAtpCpGraph](), scores them with <code>[TAtpGnnScore]()[*model*, …]</code>, and gives the prover the new order.
- The status is `"PROVED"`, `"QUEUE_EMPTY"` (the axioms completed without proving the conjecture) or `"TIMEOUT"` (the step budget ran out).
- The ranking only changes the order in which the critical pairs are processed, and the prover still selects a pair in order of age from time to time, so every proof the prover can find is still found.
- [TFindProofGnnReranked]() ranks the queue inside the prover, with no evaluation in the Wolfram Language during the search, and is faster.
- The following options can be given:

| | | |
| --- | --- | --- |
| `"RerankPeriod"` | `200` | number of steps between two rankings |
| [MaxSteps]() | `30000` | maximum number of steps |
| `"CriticalPairWeight"` | `"Gt"` | weight that orders new critical pairs between rankings |
| `"Ordering"` | `"KBO"` | reduction ordering, `"KBO"` (Knuth-Bendix ordering) or `"LPO"` (lexicographic path ordering) |
| `"AutoPrecedence"` | [True]() | whether to choose the precedence of the symbols from the axioms |
| `"QueueCap"` | `4096` | maximum number of queued critical pairs ranked at a time |

## Basic Examples

Prove that the inverse is an involution, ranking the critical pairs with the bundled pretrained network:

```wl
TFindProofReranked[inv[inv[a]] == a, {ForAll[{x, y, z}, CircleTimes[CircleTimes[x, y], z] == CircleTimes[x, CircleTimes[y, z]]], ForAll[x, CircleTimes[x, ident] == x], ForAll[x, CircleTimes[x, inv[x]] == ident]}, TAtpLoadGnnScorer[TAtpGnnScorerAsset[]]]
```

<!-- => "PROVED" -->

## Scope

Rank with a small network written by hand, with one round and two hidden units:

```wl
TFindProofReranked[inv[inv[a]] == a, {ForAll[{x, y, z}, CircleTimes[CircleTimes[x, y], z] == CircleTimes[x, CircleTimes[y, z]]], ForAll[x, CircleTimes[x, ident] == x], ForAll[x, CircleTimes[x, inv[x]] == ident]}, <|"Kind" -> "GNN", "W1" -> {N[IdentityMatrix[{12, 2}]]}, "Ws" -> {N[IdentityMatrix[{12, 2}]]}, "Bh" -> {{0., 0.}}, "Wout" -> {{1., 0.}, {0., 1.}}, "Bout" -> {0., 0.}, "Rounds" -> 1, "Hidden" -> 2, "NMax" -> 64|>]
```

<!-- => "PROVED" -->

---

A conjecture that does not follow from the axioms ends with an empty queue:

```wl
TFindProofReranked[f[a] == a, {f[f[x_]] == x_}, <|"Kind" -> "GNN", "W1" -> {N[IdentityMatrix[{12, 2}]]}, "Ws" -> {N[IdentityMatrix[{12, 2}]]}, "Bh" -> {{0., 0.}}, "Wout" -> {{1., 0.}, {0., 1.}}, "Bout" -> {0., 0.}, "Rounds" -> 1, "Hidden" -> 2, "NMax" -> 64|>]
```

<!-- => "QUEUE_EMPTY" -->

## Options

### RerankPeriod

Rank the queue every five steps:

```wl
TFindProofReranked[inv[inv[a]] == a, {ForAll[{x, y, z}, CircleTimes[CircleTimes[x, y], z] == CircleTimes[x, CircleTimes[y, z]]], ForAll[x, CircleTimes[x, ident] == x], ForAll[x, CircleTimes[x, inv[x]] == ident]}, <|"Kind" -> "GNN", "W1" -> {N[IdentityMatrix[{12, 2}]]}, "Ws" -> {N[IdentityMatrix[{12, 2}]]}, "Bh" -> {{0., 0.}}, "Wout" -> {{1., 0.}, {0., 1.}}, "Bout" -> {0., 0.}, "Rounds" -> 1, "Hidden" -> 2, "NMax" -> 64|>, "RerankPeriod" -> 5]
```

<!-- => "PROVED" -->

### MaxSteps

A small step budget stops the search before the proof:

```wl
TFindProofReranked[inv[inv[a]] == a, {ForAll[{x, y, z}, CircleTimes[CircleTimes[x, y], z] == CircleTimes[x, CircleTimes[y, z]]], ForAll[x, CircleTimes[x, ident] == x], ForAll[x, CircleTimes[x, inv[x]] == ident]}, <|"Kind" -> "GNN", "W1" -> {N[IdentityMatrix[{12, 2}]]}, "Ws" -> {N[IdentityMatrix[{12, 2}]]}, "Bh" -> {{0., 0.}}, "Wout" -> {{1., 0.}, {0., 1.}}, "Bout" -> {0., 0.}, "Rounds" -> 1, "Hidden" -> 2, "NMax" -> 64|>, MaxSteps -> 5]
```

<!-- => "TIMEOUT" -->

### CriticalPairWeight

Order new critical pairs by their size:

```wl
TFindProofReranked[inv[inv[a]] == a, {ForAll[{x, y, z}, CircleTimes[CircleTimes[x, y], z] == CircleTimes[x, CircleTimes[y, z]]], ForAll[x, CircleTimes[x, ident] == x], ForAll[x, CircleTimes[x, inv[x]] == ident]}, <|"Kind" -> "GNN", "W1" -> {N[IdentityMatrix[{12, 2}]]}, "Ws" -> {N[IdentityMatrix[{12, 2}]]}, "Bh" -> {{0., 0.}}, "Wout" -> {{1., 0.}, {0., 1.}}, "Bout" -> {0., 0.}, "Rounds" -> 1, "Hidden" -> 2, "NMax" -> 64|>, "CriticalPairWeight" -> "Add"]
```

<!-- => "PROVED" -->

### Ordering

Orient the rules with the lexicographic path ordering:

```wl
TFindProofReranked[inv[inv[a]] == a, {ForAll[{x, y, z}, CircleTimes[CircleTimes[x, y], z] == CircleTimes[x, CircleTimes[y, z]]], ForAll[x, CircleTimes[x, ident] == x], ForAll[x, CircleTimes[x, inv[x]] == ident]}, <|"Kind" -> "GNN", "W1" -> {N[IdentityMatrix[{12, 2}]]}, "Ws" -> {N[IdentityMatrix[{12, 2}]]}, "Bh" -> {{0., 0.}}, "Wout" -> {{1., 0.}, {0., 1.}}, "Bout" -> {0., 0.}, "Rounds" -> 1, "Hidden" -> 2, "NMax" -> 64|>, "Ordering" -> "LPO"]
```

<!-- => "PROVED" -->

### AutoPrecedence

Keep the default precedence of the symbols:

```wl
TFindProofReranked[inv[inv[a]] == a, {ForAll[{x, y, z}, CircleTimes[CircleTimes[x, y], z] == CircleTimes[x, CircleTimes[y, z]]], ForAll[x, CircleTimes[x, ident] == x], ForAll[x, CircleTimes[x, inv[x]] == ident]}, <|"Kind" -> "GNN", "W1" -> {N[IdentityMatrix[{12, 2}]]}, "Ws" -> {N[IdentityMatrix[{12, 2}]]}, "Bh" -> {{0., 0.}}, "Wout" -> {{1., 0.}, {0., 1.}}, "Bout" -> {0., 0.}, "Rounds" -> 1, "Hidden" -> 2, "NMax" -> 64|>, "AutoPrecedence" -> False]
```

<!-- => "PROVED" -->

### QueueCap

Rank at most two queued critical pairs at a time:

```wl
TFindProofReranked[inv[inv[a]] == a, {ForAll[{x, y, z}, CircleTimes[CircleTimes[x, y], z] == CircleTimes[x, CircleTimes[y, z]]], ForAll[x, CircleTimes[x, ident] == x], ForAll[x, CircleTimes[x, inv[x]] == ident]}, <|"Kind" -> "GNN", "W1" -> {N[IdentityMatrix[{12, 2}]]}, "Ws" -> {N[IdentityMatrix[{12, 2}]]}, "Bh" -> {{0., 0.}}, "Wout" -> {{1., 0.}, {0., 1.}}, "Bout" -> {0., 0.}, "Rounds" -> 1, "Hidden" -> 2, "NMax" -> 64|>, "QueueCap" -> 2]
```

<!-- => "PROVED" -->

## Properties and Relations

[TFindProof]() proves the same theorem without a network:

```wl
TFindProof[inv[inv[a]] == a, {ForAll[{x, y, z}, CircleTimes[CircleTimes[x, y], z] == CircleTimes[x, CircleTimes[y, z]]], ForAll[x, CircleTimes[x, ident] == x], ForAll[x, CircleTimes[x, inv[x]] == ident]}, "Status"]
```

<!-- => "Proved" -->

---

[TFindProofGnnReranked]() ranks with the same network:

```wl
TFindProofGnnReranked[inv[inv[a]] == a, {ForAll[{x, y, z}, CircleTimes[CircleTimes[x, y], z] == CircleTimes[x, CircleTimes[y, z]]], ForAll[x, CircleTimes[x, ident] == x], ForAll[x, CircleTimes[x, inv[x]] == ident]}, <|"Kind" -> "GNN", "W1" -> {N[IdentityMatrix[{12, 2}]]}, "Ws" -> {N[IdentityMatrix[{12, 2}]]}, "Bh" -> {{0., 0.}}, "Wout" -> {{1., 0.}, {0., 1.}}, "Bout" -> {0., 0.}, "Rounds" -> 1, "Hidden" -> 2, "NMax" -> 64|>]
```

<!-- => "PROVED" -->
