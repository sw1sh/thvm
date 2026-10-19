---
Template: Symbol
Name: THeapGraph
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/THeapGraph
Keywords: [interaction net, heap, visualization, graph, agents, wires]
SeeAlso: [THeapDiagram, TScheduleGraph, TCausalGraph, TTermExpr, Term]
RelatedGuides: [THVMLink]
---

## Usage

<code>[THeapGraph]()[*t*]</code> gives a graph of the agents of the dynamic heap that the term *t* reaches.

<code>[THeapGraph]()[{$t_1$, $t_2$, …}]</code> gives a graph of the agents that any of the terms $t_i$ reaches.

<code>[THeapGraph]()[]</code> gives a graph of every agent stored on the dynamic heap.

<!-- #| annotation: 26.10.19: THeapGraph is the Graph-based renderer in Visualization.wl. The seeded forms walk from the given terms (reachableICAgents), so a compound the WL side holds directly, such as an OP2 built by the arithmetic sugar, is drawn; the no-argument form scans the cells from THeapBase[] to THeapPos[] - 1 and only finds agents some cell points to. Edges run in the direction of data flow (source to consumer), the reverse of the heap pointers, and carry their port name as an edge tag even when "ShowEdgeLabels" is False, so parallel wires stay distinct. VAR and DP slots whose binder cell is SUB-flagged are drawn through to the substituted value. UOP_KERNEL cells expose their TKernelInputs as input edges, matching TScheduleGraph. The palette and shapes come from Style.wl. -->

## Details & Options

- The graph draws the term as an interaction net: one vertex for each agent, such as a lambda, an application, a superposition, a duplication, a constructor, a matcher, an arithmetic node, a number, a tensor or a UOp (micro-operation) node, and one edge for each wire.
- An edge runs from a term to the agent that holds it, in the direction in which values flow, so the root of the term is at the bottom.
- Each vertex is labeled with its tag and heap location, and each kind of agent has its own shape and color.
- Only the agents reachable from the seed terms are drawn. Without a seed, the graph covers the cells from <code>[THeapBase]()[]</code> to <code>[THeapPos]()[]</code> - 1, including the leftovers of earlier reductions, and misses a node that only the Wolfram Language holds.
- The default layout is `"LayeredDigraphEmbedding"`.
- [THeapGraph]() takes the options of [Graph](), such as [ImageSize]() and [GraphLayout](), and the following:

| | | |
| --- | --- | --- |
| `"ShowEdgeLabels"` | [False]() | whether to label each wire with the name of its port |

## Basic Examples

The interaction net of the successor function applied to 41:

```wl
THeapGraph[TLam[x, x + 1][TNum[41]], ImageSize -> 300]
```
<!-- => a Graph of 5 vertices: the numbers 1 and 41, the OP2 + body wired to and from the LAM, and the APP at the bottom fed by the LAM and by 41 -->

## Scope

A superposition with 3 added to it:

```wl
THeapGraph[TSup[1, 2] + 3, ImageSize -> 300, AspectRatio -> 1]
```
<!-- => a Graph of 5 vertices: the numbers 1 and 2 feed the SUP, which feeds the OP2 + together with 3 -->

---

A lambda over a tensor, whose body is a UOp node:

```wl
THeapGraph[TApp[TLam[w, TUOpMul[w, w]], TTensorCreate[{1., 2., 3.}]], ImageSize -> 300, AspectRatio -> 1]
```
<!-- => a Graph of 4 vertices: the MUL UOp node feeds the LAM, and the LAM and the tensor t1 feed the APP -->

---

Draw two terms in one graph:

```wl
THeapGraph[{TSup[1, 2], TLam[x, x]}, ImageSize -> 300, AspectRatio -> 4/5]
```
<!-- => a Graph of 4 vertices: the SUP fed by 1 and 2, and the identity LAM with a wire from itself to itself -->

---

A tensor shared by two terms appears once, with a wire to each use:

```wl
shared = TTensorCreate[{1., 2., 3., 4.}]
```
<!-- => a TTensor summary box with shape 4 and dtype f32 -->

The sum and the product both read the one tensor:

```wl
THeapGraph[{shared + shared, shared * shared}, ImageSize -> 300, AspectRatio -> 1]
```
<!-- => a Graph of 3 vertices: one tensor vertex with two wires into an ADD node and two into a MUL node -->

---

A duplication shares one value between its two projections:

```wl
THeapGraph[Plus @@ TDup[TSup[1, 2]], ImageSize -> 300, AspectRatio -> 2/3]
```
<!-- => a Graph of 5 vertices: 1 and 2 feed the SUP, the SUP feeds the DUP, and two wires run from the DUP into the OP2 + -->

---

A conditional is an application of a matcher:

```wl
THeapGraph[TIfZero[TNum[0], TNum[10], TNum[20]], ImageSize -> 300, AspectRatio -> 2/3]
```
<!-- => a Graph of 6 vertices: the MAT#0 matcher, the discarding LAM, the APP and the numbers 0, 10 and 20 -->

---

Start from an empty heap:

```wl
TReset[];
```

Build a term, keeping no seed:

```wl
TSup[1, 2] + 3
```
<!-- => a TTerm summary box with tag OP2 -->

Without a seed the graph shows what the heap cells point to, which leaves out the addition node held only by the Wolfram Language:

```wl
THeapGraph[ImageSize -> 300]
```
<!-- => a Graph of 3 vertices: the SUP fed by 1 and 2 -->

## Options

### ShowEdgeLabels

Label each wire with the port it enters:

```wl
THeapGraph[TSup[1, 2] + 3, "ShowEdgeLabels" -> True, ImageSize -> 300, AspectRatio -> 1]
```
<!-- => the SUP + 3 Graph with the wires labeled L and R -->

### GraphLayout

Choose another layout for the same net:

```wl
THeapGraph[TLam[x, x + 1][TNum[41]], GraphLayout -> "SpringElectricalEmbedding", ImageSize -> 300]
```
<!-- => the 5-vertex Graph of the successor application in a spring-electrical layout -->

## Applications

Compare a term before and after reduction:

```wl
t = TLam[x, x + 1][TNum[41]]
```
<!-- => a TTerm summary box with tag APP -->

Before the reduction the application and the lambda are both present:

```wl
THeapGraph[t, ImageSize -> 300]
```
<!-- => a Graph of 5 vertices with the APP and the LAM -->

Reduce the term:

```wl
r = TWnf[t]
```
<!-- => a TNum summary box showing the number 42 -->

The result is a single number:

```wl
THeapGraph[r, ImageSize -> 150]
```
<!-- => a Graph with the single vertex 42 -->

## Properties and Relations

The graph is an ordinary [Graph]():

```wl
VertexCount[THeapGraph[TLam[x, x + 1][TNum[41]]]]
```
<!-- => 5 -->

---

<code>[Term]()</code> gives the same term as a nested expression:

```wl
Term[TLam[x, x + 1][TNum[41]]]
```
<!-- => Term["APP", Term["LAM", id, Term["OP2", "+", Term["VAR", id], Term["NUM", 1]]], Term["NUM", 41]] -->

## Possible Issues

Reduction rewrites the cells of a term, so the original application is gone afterwards and reaches nothing:

```wl
t = TLam[x, x + 1][TNum[41]]
```
<!-- => a TTerm summary box with tag APP -->

Reduce it in place:

```wl
TReduce[t]
```
<!-- => the same TTerm summary box, with tag APP -->

The consumed application draws as an empty graph; draw the result of the reduction instead:

```wl
THeapGraph[t]
```
<!-- => an empty Graph -->
