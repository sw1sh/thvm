---
Template: Symbol
Name: TCausalGraph
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TCausalGraph
Keywords: [causal, multicomputation, trace, dag, wire provenance, hvm4]
SeeAlso: [TMultiTrace, TMultiwayGraph, TCollapse]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TCausalGraph]()[*input*]</code> returns a directed [Graph]() of the causal structure of a reduction. *input* is either a `trace` (the flat list of event Associations <code>[TMultiTrace]()[t][[All, "Events"]]</code>) or a `steps` list (events are flattened across all step records).

<code>[TCausalGraph]()[*expr*]</code> runs [TMultiTrace]() internally for convenience.

## Details & Options

- Vertices are event ids; an edge `F -> E` is drawn iff `F.id` appears in `E.consumed` -- this is the wire-provenance link recorded by the M1 trace layer (see `docs/multicomputation.md` section 3.1).
- Each vertex is coloured by its event's family (`TERM`, `SLIDE`, `FORK`, `SPLIT`, `MERGE`, `PRUNE`, `DIST`).
- `"VertexLabels" -> Automatic` labels each event with its rule name.
- `"Family" -> {"TERM", ...}` restricts the view to events of the listed families.
- `"PlotLegends" -> Automatic` emits a `SwatchLegend` of family -> colour (default `None`).
- Sharing -- a single producer feeding several consumers -- shows up as a fan-out vertex; the causal graph is therefore a DAG, not a tree.

## Basic Examples

The causal DAG for a small commute-and-fold:

```wl
TReset[];
TCausalGraph[TMultiTrace[TSup[1, 2] + 3]]
```

## Properties and Relations

[TMultiwayGraph]() draws the *state* view (vertices = canonical slices). `TCausalGraph` draws the *event* view (vertices = interaction ids). Both views read from the same trace.
