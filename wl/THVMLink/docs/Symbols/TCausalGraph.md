---
Template: Symbol
Name: TCausalGraph
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TCausalGraph
Keywords: [causal graph, multicomputation, trace, events, wire provenance, interaction combinator]
SeeAlso: [TMultiTrace, TMultiwayGraph, TMultiTraceQ, TCollapse, TItrs]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TCausalGraph]()[*t*]</code> gives the causal graph of the interactions that reduce the term *t*.

<code>[TCausalGraph]()[*t*, *max*]</code> follows at most *max* steps of the reduction.

<code>[TCausalGraph]()[*events*]</code> gives the causal graph of a list of trace events, or of a list of steps from <code>[TMultiTrace]()</code>.

<!-- #| annotation: 26.10.19: The event records come from the C trace layer (built with -DTHVM_TRACE, the default for make wl); each event lists the producer event ids whose wires it consumed, and TCausalGraph draws exactly that relation, then applies TransitiveReductionGraph by default to show direct dependencies only. The layout is fixed to LayeredDigraphEmbedding, and the user's Graph options are placed before the defaults so they win. The family palette is shared with TMultiwayGraph's edge colours. Prior art: the causal graphs of the Wolfram Physics Project and the Multicomputation paclet, here built from interaction events of an interaction-net reducer rather than from rewrite events of a hypergraph or string system. -->

## Details & Options

- The result is a directed [Graph]() with one vertex per interaction, labeled with the name of its rule, and with the vertices numbered in the order the interactions fire.
- An edge from one event to another means the second consumed a wire the first produced. The causal graph is acyclic, and a producer that feeds several consumers has several outgoing edges.
- Each vertex is colored by the family of its rule: `TERM`, `SLIDE`, `FORK`, `SPLIT`, `MERGE`, `PRUNE` or `DIST`.
- A term *t* is reduced by <code>[TMultiTrace]()</code>, so the graph covers the whole reduction and the term is consumed.
- The list *events* is either a flat list of event associations or a list of step associations, each with an `"Events"` key, as <code>[TMultiTrace]()</code> gives them.
- The trace is recorded by the runtime when <code>[TMultiTraceQ]()</code> gives [True](), as it does for the default build.
- [TCausalGraph]() takes the options of [Graph](), such as [ImageSize]() and [VertexLabels](), and the following:

| | | |
| --- | --- | --- |
| `"Family"` | [All]() | the rule families whose events are kept |
| `"TransitiveReduction"` | [True]() | whether to drop edges implied by longer paths |
| [PlotLegends]() | [None]() | whether to add a legend of the family colors |

## Basic Examples

The causal graph of squaring a superposition of 5 and 7:

```wl
TCausalGraph[TLam[x, x * x][TSup[5, 7]], AspectRatio -> 1/2, ImageSize -> 360]
```
<!-- => a Graph of 8 events: APP_LAM at the top, DUP_SUP_COM fanning out to DUP_NUM, DUP_NUM and OP2_SUP, a DUP_SUP_ANN, and two OP2_NUM_NUM events at the bottom -->

## Scope

Adding 3 to a superposition fires one `OP2_SUP` event, on which the rest depend:

```wl
TCausalGraph[TSup[1, 2] + 3, AspectRatio -> 1/2, ImageSize -> 300]
```
<!-- => a Graph of 4 events: OP2_SUP, then DUP_NUM, then two OP2_NUM_NUM events -->

---

A beta reduction followed by an addition is a chain of two events:

```wl
TCausalGraph[TLam[x, x + 1][TNum[41]], AspectRatio -> 1/4, ImageSize -> 300]
```
<!-- => a Graph with an edge from APP_LAM to OP2_NUM_NUM -->

---

Follow only the first two steps of a reduction:

```wl
TCausalGraph[TLam[x, x * x][TSup[5, 7]], 2, AspectRatio -> 1/4, ImageSize -> 300]
```
<!-- => a Graph with an edge from APP_LAM to DUP_SUP_COM -->

---

The steps of a <code>[TMultiTrace]()</code> give the same graph:

```wl
TCausalGraph[TMultiTrace[TSup[1, 2] + 3, {"Events"}], AspectRatio -> 1/2, ImageSize -> 300]
```
<!-- => the same 4-event Graph as for TSup[1, 2] + 3 -->

---

So does the flat list of its events:

```wl
TCausalGraph[Catenate[TMultiTrace[TSup[1, 2] + 3, "Events"]], AspectRatio -> 1/2, ImageSize -> 300]
```
<!-- => the same 4-event Graph as for TSup[1, 2] + 3 -->

---

Adding two superpositions ends in four additions, one for each pair of branches:

```wl
TCausalGraph[TSup[1, 2] + TSup[10, 20], AspectRatio -> 1/2, ImageSize -> 400]
```
<!-- => a Graph of 10 events: OP2_SUP, then DUP_SUP_COM feeding two OP2_NUM_SUP and two DUP_NUM events, then four OP2_NUM_NUM events -->

## Options

### Family

Keep only the events of the `TERM` family:

```wl
TCausalGraph[TLam[x, x * x][TSup[5, 7]], "Family" -> {"TERM"}, AspectRatio -> 1/3, ImageSize -> 300]
```
<!-- => a Graph of 3 unconnected events: APP_LAM and two OP2_NUM_NUM -->

### TransitiveReduction

Keep the edges that longer paths already imply:

```wl
TCausalGraph[TLam[x, x * x][TSup[5, 7]], "TransitiveReduction" -> False, AspectRatio -> 1/2, ImageSize -> 360]
```
<!-- => a Graph of 8 events and 10 edges, against 8 edges by default -->

### PlotLegends

Add a legend of the family colors:

```wl
TCausalGraph[TLam[x, x * x][TSup[5, 7]], PlotLegends -> Automatic, AspectRatio -> 1/2, ImageSize -> 360]
```
<!-- => the 8-event Graph with a "family" swatch legend for TERM, SPLIT, SLIDE, DIST and MERGE -->

## Properties and Relations

<code>[TMultiwayGraph]()</code> draws the same reduction as a graph of states joined by events:

```wl
TMultiwayGraph[TSup[1, 2] + 3, ImageSize -> 300]
```
<!-- => a Graph from <1, 2> + 3 through OP2_SUP, DUP_NUM and OP2_NUM_NUM edges to the states 4 and 5 -->

---

Each vertex is one interaction, so the vertex count is the number of interactions fired:

```wl
VertexCount[TCausalGraph[TLam[x, x * x][TSup[5, 7]]]]
```
<!-- => 8 -->

## Possible Issues

A term already in normal form fires no interactions, so its causal graph is empty:

```wl
TCausalGraph[TNum[3]]
```
<!-- => an empty Graph -->
