---
Template: Symbol
Name: TAtpCpGraphEquation
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TAtpCpGraphEquation
Keywords: [ENIGMA, graph neural network, GNN, theorem proving, equation graph, decoder]
SeeAlso: [TAtpCpGraph, TAtpGraphDataset, TAtpTrainGnn]
RelatedGuides: [THVMLink]
RelatedTutorials: ["[Learned Guidance: a GNN for Critical-Pair Selection](GnnGuidance)"]
---

## Usage

<code>[TAtpCpGraphEquation]()[*graph*]</code> rebuilds the equation encoded by a [TAtpCpGraph]() *graph*.

## Details & Options

- [TAtpCpGraphEquation]() reads the term structure from the `"Edges"` of *graph* and the symbols from its `"Symbols"`, and gives <code>[Inactive]()[[Equal]()][*lhs*, *rhs*]</code>.
- The equation is inactive, so an equation with two equal sides does not evaluate to [True]().
- A graph without `"Symbols"` gives `$Failed`.

## Basic Examples

Rebuild an equation from its graph:

```wl
TAtpCpGraphEquation[TAtpCpGraph[CircleTimes[a, OverBar[a]] == ident]]
```

<!-- => Inactive[Equal][a ⊗ OverBar[a], ident] -->

## Scope

The order of the arguments is kept:

```wl
TAtpCpGraphEquation[TAtpCpGraph[f[a, b] == f[b, a]]]
```

<!-- => Inactive[Equal][f[a, b], f[b, a]] -->

---

An equation with two equal sides stays inactive:

```wl
TAtpCpGraphEquation[TAtpCpGraph[Inactive[Equal][a, a]]]
```

<!-- => Inactive[Equal][a, a] -->

## Properties and Relations

[Activate]() gives the equation:

```wl
Activate[TAtpCpGraphEquation[TAtpCpGraph[f[a, b] == f[b, a]]]]
```

<!-- => f[a, b] == f[b, a] -->

## Possible Issues

A graph without its symbols gives `$Failed`:

```wl
TAtpCpGraphEquation[KeyDrop[TAtpCpGraph[CircleTimes[a, OverBar[a]] == ident], "Symbols"]]
```

<!-- => $Failed -->
