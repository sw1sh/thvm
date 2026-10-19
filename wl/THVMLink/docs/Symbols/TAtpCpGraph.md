---
Template: Symbol
Name: TAtpCpGraph
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TAtpCpGraph
Keywords: [ENIGMA, graph neural network, GNN, theorem proving, critical pair, equation graph, learned guidance]
SeeAlso: [TAtpCpGraphEquation, TAtpGraphDataset, TAtpTrainGnn, TAtpGnnScore, TFindProof]
RelatedGuides: [THVMLink]
RelatedTutorials: ["[Learned Guidance: a GNN for Critical-Pair Selection](GnnGuidance)"]
---

## Usage

<code>[TAtpCpGraph]()[*lhs* == *rhs*]</code> encodes the equation *lhs* == *rhs* as a graph for a graph neural network that ranks the critical pairs of a proof search.

## Details & Options

- [TAtpCpGraph]() gives an Association with the keys `"NodeTypes"`, `"NodeFeatures"`, `"Edges"`, `"NNodes"`, `"NEdges"`, `"NodeLabels"` and `"Symbols"`.
- Node `0` stands for the whole equation. The other nodes stand for the occurrences of subterms of *lhs* and then *rhs*, for the distinct operators and constants, and for the distinct variables. Their `"NodeTypes"` are `0` for the equation, `1` for a subterm, `2` for an operator or constant and `3` for a variable.
- `"Edges"` is a list of triples `{`*from*`, `*to*`, `*type*`}` that link a subterm to its head and its arguments, and the equation to its two sides.
- `"NodeFeatures"` has one row of 12 numbers for each node, which describe its kind, arity and number of occurrences and do not depend on the names of operators, constants and variables. Equations that differ only by a renaming of their symbols give the same graph, apart from `"NodeLabels"` and `"Symbols"`.
- `"NodeLabels"` and `"Symbols"` record the symbol of each node, from which [TAtpCpGraphEquation]() rebuilds the equation.
- The equation can also be given as <code>[Inactive]()[[Equal]()][*lhs*, *rhs*]</code> or wrapped in [HoldForm]().
- [TAtpGraphDataset]() encodes the lemmas of proofs this way, and [TAtpTrainGnn]() trains a network on the graphs.

## Basic Examples

The graph of the equation `f[a] == a`:

```wl
TAtpCpGraph[f[a] == a]
```

<!-- => <|"NodeTypes" -> {0, 1, 2, 1, 2, 1}, "NodeFeatures" -> {{0., 0., 0., 0., 1., 1., ...}, ...}, "Edges" -> {...}, "NodeLabels" -> {...}, "NNodes" -> 6, "NEdges" -> 6, "Symbols" -> {Missing["NotASymbol"], Missing["NotASymbol"], "f", Missing["NotASymbol"], "a", Missing["NotASymbol"]}|> -->

## Scope

The graph of an equation of group theory:

```wl
g = TAtpCpGraph[CircleTimes[a, OverBar[a]] == ident]
```

<!-- => <|"NodeTypes" -> {0, 1, 2, 1, 2, 1, 2, 1, 1, 2}, "NodeFeatures" -> {...}, "Edges" -> {{1, 2, 0}, {3, 4, 0}, {1, 3, 1}, {5, 6, 0}, {7, 4, 0}, {5, 7, 1}, {1, 5, 1}, {8, 9, 0}, {0, 1, 2}, {0, 8, 3}}, "NodeLabels" -> {0, 0, 3, 0, 4, 0, 5, 0, 0, 6}, "NNodes" -> 10, "NEdges" -> 10, "Symbols" -> {...}|> -->

The type of each node:

```wl
g["NodeTypes"]
```

<!-- => {0, 1, 2, 1, 2, 1, 2, 1, 1, 2} -->

The edges:

```wl
g["Edges"]
```

<!-- => {{1, 2, 0}, {3, 4, 0}, {1, 3, 1}, {5, 6, 0}, {7, 4, 0}, {5, 7, 1}, {1, 5, 1}, {8, 9, 0}, {0, 1, 2}, {0, 8, 3}} -->

The symbol of each node:

```wl
g["Symbols"]
```

<!-- => {Missing["NotASymbol"], Missing["NotASymbol"], "CircleTimes", Missing["NotASymbol"], "a", Missing["NotASymbol"], "OverBar", Missing["NotASymbol"], Missing["NotASymbol"], "ident"} -->

---

A variable gets a node of type `3`:

```wl
TAtpCpGraph[f[x_] == x_]["NodeTypes"]
```

<!-- => {0, 1, 2, 1, 3, 1} -->

---

An inactive equation is accepted:

```wl
TAtpCpGraph[Inactive[Equal][a, a]]["NNodes"]
```

<!-- => 4 -->

## Properties and Relations

Renaming the symbols of an equation does not change its graph, apart from the labels:

```wl
KeyDrop[TAtpCpGraph[CircleTimes[x, OverBar[x]] == ident], {"NodeLabels", "Symbols"}] === KeyDrop[TAtpCpGraph[p[y, q[y]] == r], {"NodeLabels", "Symbols"}]
```

<!-- => True -->

---

[TAtpCpGraphEquation]() rebuilds the equation from its graph:

```wl
TAtpCpGraphEquation[TAtpCpGraph[CircleTimes[a, OverBar[a]] == ident]]
```

<!-- => Inactive[Equal][a ⊗ OverBar[a], ident] -->

## Possible Issues

An expression that is not an equation gives `$Failed`:

```wl
TAtpCpGraph[a]
```

<!-- => $Failed -->
