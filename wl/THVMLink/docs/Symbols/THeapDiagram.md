---
Template: Symbol
Name: THeapDiagram
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/THeapDiagram
Keywords: [diagram, heap, string diagram, interaction net, DiagramNetwork, port]
SeeAlso: [THeapGraph, TMultiwayGraph, TCollapse, TSup]
RelatedGuides: [THVMLink]
---

## Usage

<code>[THeapDiagram]()[*term*]</code> builds a `Wolfram`DiagrammaticComputation``DiagramNetwork` from the heap, with one [Diagram]() per compound agent and one `ERA`/`TEN`/`NUM`/... leaf for each atom referenced from a rendered slot.

<code>[THeapDiagram]()[{*t*<sub>1</sub>, *t*<sub>2</sub>, ...}]</code> seeds discovery with several roots.

## Details & Options

- Wires share string identifiers keyed off heap loc; `VAR` cells collapse to their binder loc so binder and bound variable share a single wire.
- DUP projections render via `tenTid`-style ports so the two arms are visually distinct from a plain DP-projection's heap wire.
- Default mode is *reachable*: only cells reachable from the seed terms appear. This keeps stale cells from prior reductions out of the picture.
- Pair with the `DiagrammaticComputation` API (`DiagramCases`, `DiagramPattern`, port `"Name"`) to query the rendered network.

## Basic Examples

The string diagram of a small superposition + arithmetic:

```wl
TReset[];
THeapDiagram[TSup[1, 2] + 3]
```

## Properties and Relations

[THeapGraph]() is the lower-level graph view (vertices = cells, edges = wires); `THeapDiagram` is the *string-diagram* view that renders each agent as a typed box with named ports and uses `DiagramNetwork` for layout. The interactive `Wolfram`DiagrammaticComputation`` library exposes inspection helpers.
