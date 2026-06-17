---
Template: Symbol
Name: TMultiwayGraph
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TMultiwayGraph
Keywords: [multiway, multicomputation, slice, branchial, hvm4, WPP]
SeeAlso: [TMultiTrace, TCausalGraph, TCollapse, TSup]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TMultiwayGraph]()[*steps*]</code> returns the multiway view of a [TMultiTrace]() reduction. *steps* is the per-step `List` of `Association`s (each carrying `"CanonicalSlice"`, `"SliceBoxes"`, `"Events"`).

<code>[TMultiwayGraph]()[*expr*]</code> runs [TMultiTrace]() internally and returns the graph -- a convenience for one-shot use.

## Details & Options

- Vertices are *TERM-slices*: per step the active term's `SUP`-head is unfolded so a head `SUP{a, b}` contributes one vertex per leaf (recursively, until non-SUP heads), and a non-SUP head contributes one vertex.
- Edges represent trace events between consecutive steps; pairs source -> target are matched by canonical equality (unchanged leaves stay put; the residual cross-product attributes the firing only to leaves that actually transformed).
- Edges are coloured by the firing event's family: `TERM`, `SLIDE`, `FORK`, `SPLIT`, `MERGE`, `PRUNE`, `DIST` (see `docs/multicomputation.md`).
- `"Branchial" -> True` overlays a dashed branchial clique connecting the *new sibling cohort* introduced by `SLIDE` / `SPLIT` firings (the Wolfram Physics Project branchial graph aesthetic). Default `False`.
- `"VertexLabels" -> Automatic` labels each vertex with the leaf term's tag/value (the canonical [Term]()-form rendered as `TraditionalForm`).
- `"PlotLegends" -> Automatic` emits a `SwatchLegend` of family -> colour (default `None`).

## Basic Examples

The multiway view of `TSup[1, 2] + 3`:

```wl
TReset[];
TMultiwayGraph[TMultiTrace[TSup[1, 2] + 3]]
```

## Scope

Switch on the branchial overlay and the legend:

```wl
TReset[];
TMultiwayGraph[TMultiTrace[TSup[1, 2] + 3], "Branchial" -> True, "PlotLegends" -> Automatic]
```

## Properties and Relations

For the event-id causal DAG of the same trace use [TCausalGraph](). The full worked example -- including how shared variables produce `DUP-SUP-ANN` (`MERGE`) edges and independent dimensions produce `DUP-SUP-COM` (`SPLIT`) -- is in `docs/essays/boolean_superposition.md`.
