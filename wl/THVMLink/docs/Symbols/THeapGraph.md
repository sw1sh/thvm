---
Template: Symbol
Name: THeapGraph
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/THeapGraph
Keywords: [interaction net, IC, heap, visualization, string diagram]
SeeAlso: [THeapDiagram, THeap, TScheduleGraph, TMemoryPlanGantt, TTermExpr]
RelatedGuides: [THVMLink]
---

## Usage

<code>[THeapGraph]()[]</code> renders the live `thvm` heap as a Graph in the IC string-diagram style.

<code>[THeapGraph]()[*term*]</code> additionally seeds the discovery walk with *term*, so heapless compounds the caller is holding directly are included.

<code>[THeapGraph]()[{*t*<sub>1</sub>, *t*<sub>2</sub>, ...}]</code> seeds with several terms.

## Details & Options

- Walks the active context's heap from base to next-loc, plus the seed roots, and draws each combinator agent (LAM, APP, SUP, DUP, ERA) plus each tensor leaf (TEN) and UOp node as a vertex; principal-port wires become edges.
- Colours route through `Style.wl`: APP / LAM use `StandardBlue`, SUP / DUP use `StandardPurple`, TEN uses `StandardOrange`, UOP nodes use `StandardGreen`, ERA uses `StandardGray`. Light and dark modes adapt automatically.
- Accepts every `Graph` option (e.g. `GraphLayout`, `ImageSize`) - forced styling overrides win where they conflict, user options pass through otherwise.

## Basic Examples

Render the heap for a small lambda application. The `TUOpAdd[x, x]` is a <code>[TLam]()</code> binder body, where the held `x` is not a `TTerm`, so the raw UOP constructor is required:

```wl
THeapGraph @ TApp[TLam[x, TUOpAdd[x, x]], TTensorCreate[{1., 2., 3.}]]
```
<!-- => a small Graph (4 vertices): APP -> LAM, LAM body = UOP_ADD over a VAR pair, TEN leaf attached -->

## Scope

Seed with multiple roots to render a shared subgraph between two terms:

```wl
shared = TTensorCreate[{1., 2., 3., 4.}];
THeapGraph[ {shared + shared, shared*shared} ]
```
<!-- => one TEN vertex with three inbound UOP edges (ADD reads it twice + MUL reads it) -->

## Applications

Combine with <code>[TReduce]()</code> to compare a heap before and after a step. Before the step, the application is still a redex:

```wl
t = TApp[TLam[w, TUOpAdd[w, w]], TTensorCreate[{1., 2., 3.}]];
THeapGraph[t]
```
<!-- => a Graph with the APP -> LAM redex still intact over the TEN leaf -->

Fire one reduction step, then render again; the LAM and APP collapse into the inlined body, leaving the kernel + TEN:

```wl
TReduce[t];
THeapGraph[t]
```
<!-- => a Graph where the redex has fired: only the inlined UOP body + TEN survive -->

## Properties and Relations

[THeapGraph]() and <code>[THeapDiagram]()</code> draw the same underlying agent graph; <code>[THeapGraph]()</code> returns a plain `Graph`:

```wl
Head @ THeapGraph[ TLam[x, x] ]
```
<!-- => Graph -->

while <code>[THeapDiagram]()</code> routes through the `DiagrammaticComputation` paclet, so wires are typed diagram strings and the head is a `Diagram`:

```wl
Head @ THeapDiagram[ TLam[x, x] ]
```
<!-- => Diagram -->

## Possible Issues

An un-seeded call walks the whole live heap, whose size grows with accumulated runtime state:

```wl
t = TLam[x, TUOpMul[x, x]];
Length @ VertexList @ THeapGraph[]
```
<!-- => n -- the full live-heap vertex count, varies with accumulated state -->

Seeding scopes the walk to the term you are holding, so the graph is just that term's cells. Always seed when you want to visualize a specific compound:

```wl
Length @ VertexList @ THeapGraph[t]
```
<!-- => 2 -- only the cells reachable from the seed -->

## Neat Examples

The IC representation surfaces sharing visually - DUP makes a single source feed two consumers:

```wl
{x0, x1} = TDup @ TTensorCreate[{1., 2., 3.}];
THeapGraph[ x0 + x1 ]
```
<!-- => one TEN, one DUP, two DP0 / DP1 projections, one UOP_ADD -->
