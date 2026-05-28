---
Template: Symbol
Name: THeapGraph
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/THeapGraph
Keywords: [interaction net, IC, heap, visualization, string diagram]
SeeAlso: [THeapDiagram, THeap, TScheduleGraph, TMemoryPlanGantt, TTermExpr]
RelatedGuides: [THVMLink]
---

## Usage

<code>[THeapGraph]()[]</code> renders the live `thvm` heap as a Graph in the IC string-diagram style.

<code>[THeapGraph]()[$term$]</code> additionally seeds the discovery walk with $term$, so heapless compounds the caller is holding directly are included.

<code>[THeapGraph]()[$\{t_1, t_2, \ldots\}$]</code> seeds with several terms.

## Details & Options

- Walks the active context's heap from base to next-loc, plus the seed roots, and draws each combinator agent (LAM, APP, SUP, DUP, ERA) plus each tensor leaf (TEN) and UOp node as a vertex; principal-port wires become edges.
- Colours route through `Style.wl`: APP / LAM use `StandardBlue`, SUP / DUP use `StandardPurple`, TEN uses `StandardOrange`, UOP nodes use `StandardGreen`, ERA uses `StandardGray`. Light and dark modes adapt automatically.
- Accepts every `Graph` option (e.g. `GraphLayout`, `ImageSize`) - forced styling overrides win where they conflict, user options pass through otherwise.

## Basic Examples

Render the heap for a small lambda application:

```wl
Needs["THVMLink`"];
TInit[];
THeapGraph @ TApp[TLam[x, TUOpAdd[x, x]], TTensorCreate[{1., 2., 3.}]]
```
<!-- => a small Graph: APP -> LAM, LAM body = UOP_ADD over a VAR pair, TEN leaf attached -->

## Scope

Seed with multiple roots to render a shared subgraph between two terms:

```wl
shared = TTensorCreate[{1., 2., 3., 4.}];
THeapGraph[ {TUOpAdd[shared, shared], TUOpMul[shared, shared]} ]
```
<!-- => one TEN vertex with three inbound UOP edges (ADD reads it twice + MUL reads it) -->

## Applications

Combine with <code>[TReduce]()</code> to compare a heap before and after a step:

```wl
t = TApp[TLam[w, TUOpAdd[w, w]], TTensorCreate[{1., 2., 3.}]];
before = THeapGraph[t];
TReduce[t];
after  = THeapGraph[t];
{before, after}
```
<!-- => the LAM and APP collapse into the inlined body; only the kernel + TEN survive on the right -->

## Properties and Relations

`THeapGraph` and <code>[THeapDiagram]()</code> draw the same underlying agent graph; `THeapDiagram` routes through the `DiagrammaticComputation` paclet so wires are typed diagram strings:

```wl
{
    Head @ THeapGraph[ TLam[x, x] ],
    Head @ THeapDiagram[ TLam[x, x] ]
}
```
<!-- => {Graph, DiagramNetwork} -->

## Possible Issues

Compound terms the user constructed but never wrote to the heap may not appear without a seed. Always seed the call when you want to visualize a term you are still holding:

```wl
t = TLam[x, TUOpMul[x, x]];
{ Length @ VertexList @ THeapGraph[], Length @ VertexList @ THeapGraph[t] }
```
<!-- => {n, n + k} where k counts the freshly added LAM + UOP_MUL + VAR cells -->

## Neat Examples

The IC representation surfaces sharing visually - DUP makes a single source feed two consumers:

```wl
{x0, x1} = TDup @ TTensorCreate[{1., 2., 3.}];
THeapGraph[ TUOpAdd[x0, x1] ]
```
<!-- => one TEN, one DUP, two DP0 / DP1 projections, one UOP_ADD -->
