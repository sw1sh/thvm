---
Template: Symbol
Name: TMultiwayGraph
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TMultiwayGraph
Keywords: [multiway, multicomputation, slice, branchial, superposition, graph]
SeeAlso: [TMultiTrace, TCausalGraph, TCollapse, TSup, TDup]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TMultiwayGraph]()[*t*]</code> traces the reduction of the term *t* and gives its multiway graph.

<code>[TMultiwayGraph]()[*t*, *max*]</code> traces at most *max* steps.

<code>[TMultiwayGraph]()[*steps*]</code> gives the multiway graph of the step records *steps* from [TMultiTrace]().

<!-- #| annotation: 26.10.19: A VertexLabels option is accepted but has no effect, since the graph sets its own VertexLabels before the user options. The TMultiwayGraph::needsteps text still names TMultiSteps, the former name of TMultiTrace; and a per-key list such as TMultiTrace[t, "Events"] slips past the needsteps check and floods Lookup/MapThread messages. -->

## Details & Options

- The reduction is traced as by [TMultiTrace](): the head is stepped one interaction at a time, and each branch of a superposition is followed in turn.
- Each vertex is the state of one branch, identified by its [Term]() form, so a branch that does not change in a step remains the same vertex.
- An edge joins a branch to the branch it becomes in the next step. It is labeled with the rule that fired and colored by the family of the event: `"TERM"`, `"SLIDE"`, `"FORK"`, `"SPLIT"`, `"MERGE"`, `"PRUNE"` or `"DIST"`.
- Vertices are labeled with the branches in [TraditionalForm](): a superposition shows as $\langle a, b\rangle$, and a duplication projection carries a superscript 0 or 1.
- *steps* needs the keys `"CanonicalSlice"` and `"Events"`, and `"SliceBoxes"` for the typeset labels. <code>[TMultiwayGraph]()[*t*]</code> records all three.
- [TMultiwayGraph]() accepts the options of [Graph](), such as [ImageSize]() and [AspectRatio](), and the following options:

| | | |
| --- | --- | --- |
| `"Branchial"` | [False]() | whether to join the new branches of a `"SLIDE"` or `"SPLIT"` step by dashed edges |
| [PlotLegends]() | [None]() | [Automatic]() adds a legend of the event families |

- [TCausalGraph]() draws the events of a trace and their dependencies instead of the branch states.

## Basic Examples

The multiway graph of a sum over a superposition:

```wl
TMultiwayGraph[TSup[1, 2] + 3]
```

<!-- => a Graph from the vertex <1, 2> + 3 down two branches, OP2_SUP then DUP_NUM then OP2_NUM_NUM, ending at the vertices 4 and 5 -->

---

The same graph from the step records of [TMultiTrace]():

```wl
TMultiwayGraph[TMultiTrace[TSup[1, 2] + 3, {"Events", "CanonicalSlice", "SliceBoxes"}]]
```

<!-- => the same Graph, two branches from <1, 2> + 3 ending at 4 and 5 -->

## Scope

Only the first two steps:

```wl
TMultiwayGraph[TSup[1, 2] + 3, 2, AspectRatio -> 1/2]
```

<!-- => a Graph from <1, 2> + 3 along two OP2_SUP edges, each followed by one DUP_NUM edge -->

---

Duplicate a superposition with a different label:

```wl
{e0, e1} = TDup[2, TSup[1, 1, 2]]
```

<!-- => a list of two TTerm summary boxes, tags DP0 and DP1 -->

The sum of the two copies splits on the different label and merges on the shared one:

```wl
TMultiwayGraph[e0 + e1]
```

<!-- => a Graph that starts with DUP_SUP_COM and OP2_SUP, follows two branches through DUP_NUM, DUP_SUP_ANN, DUP_NUM and OP2_NUM_NUM, and ends at 3 and 4 -->

---

The graph of a sum over a superposition has seven branch states:

```wl
VertexCount[TMultiwayGraph[TSup[1, 2] + 3]]
```

<!-- => 7 -->

## Options

### Branchial

Join the two branches created by the first step with a dashed branchial edge:

```wl
TMultiwayGraph[TSup[1, 2] + 3, "Branchial" -> True]
```

<!-- => the multiway Graph of <1, 2> + 3 with a dashed edge between the branches 1 + 3^0 and 2 + 3^1 that the OP2_SUP step creates -->

---

The branchial edge is undirected:

```wl
Count[EdgeList[TMultiwayGraph[TSup[1, 2] + 3, "Branchial" -> True]], _UndirectedEdge]
```

<!-- => 1 -->

### PlotLegends

Add a legend of the event families:

```wl
TMultiwayGraph[TSup[1, 2] + 3, PlotLegends -> Automatic]
```

<!-- => the multiway Graph of <1, 2> + 3 with a legend for the families SLIDE, DIST and TERM -->

## Properties and Relations

[TCollapse]() lists the values at the ends of the branches:

```wl
TTermVal /@ TCollapse[TSup[1, 2] + 3]
```

<!-- => {4, 5} -->

## Possible Issues

A flat list of events has no branch states, so the message `TMultiwayGraph::needsteps` is issued and the result is `$Failed`:

```wl
TMultiwayGraph[Flatten[TMultiTrace[TSup[1, 2] + 3, "Events"]]]
```

<!-- => the message TMultiwayGraph::needsteps is issued and the result is $Failed -->
