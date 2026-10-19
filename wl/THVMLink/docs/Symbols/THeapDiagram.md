---
Template: Symbol
Name: THeapDiagram
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/THeapDiagram
Keywords: [string diagram, interaction net, heap, ports, wires, visualization]
SeeAlso: [THeapGraph, TCausalGraph, TMultiwayGraph, Term]
RelatedGuides: [THVMLink]
---

## Usage

<code>[THeapDiagram]()[*t*]</code> gives a string diagram of the agents of the dynamic heap that the term *t* reaches.

<code>[THeapDiagram]()[{$t_1$, $t_2$, …}]</code> gives a string diagram of the agents that any of the terms $t_i$ reaches.

<code>[THeapDiagram]()[]</code> gives a string diagram of every agent on the dynamic heap.

<!-- #| annotation: 26.10.19: Diagram.wl builds one DC Diagram per compound agent with one port per heap slot, plus a leaf Diagram for each atom referenced from a rendered slot, and wires them into a DiagramNetwork, which evaluates to a single Diagram. Its BeginPackage imports only the main context Wolfram`DiagrammaticComputation`; DC 1.0.8 (the version in the paclet repository on the probe machine) defines Diagram and DiagramNetwork in the subcontext Wolfram`DiagrammaticComputation`Diagram`, so with 1.0.8 the names resolve to WolframInstitute`THVMLink`Private` symbols and THeapDiagram gives an inert Private`DiagramNetwork[Private`Diagram[...]] with no message. With DC 1.0.13 (the dev checkout) every example on this page evaluates to a Diagram as the hints describe; the hints were verified that way. The page needs DC >= 1.0.13 in the build kernel, or Diagram.wl importing the subcontext. -->

## Details & Options

- The result is a diagram of the `` Wolfram`DiagrammaticComputation` `` paclet: one box for each compound agent, such as a lambda, an application, a superposition, a duplication or an arithmetic node, and one node for each atom, such as a number or an eraser, that a slot refers to.
- Each slot of an agent is a port, and ports are joined by wires named after heap locations: `w` followed by the location of the cell that holds a value, and `var` followed by the location of a lambda, for the wire of its variable. A variable and its binder therefore share one wire.
- The two projections of a duplication leave through two separately named ports, so its arms are drawn apart.
- Each box is labeled with its tag and heap location, in the colors used by <code>[THeapGraph]()</code>.
- Only the agents reachable from the seed terms are drawn. <code>[THeapDiagram]()[]</code> draws every agent on the heap that some cell refers to.
- [THeapDiagram]() requires version 1.0.13 or later of the `Wolfram/DiagrammaticComputation` paclet.

## Basic Examples

The string diagram of a superposition with 3 added to it:

```wl
THeapDiagram[TSup[1, 2] + 3]
```
<!-- => a string diagram: the numbers 1 and 2 wired into a SUP box, the SUP and the number 3 wired into an OP2 + box, and the open output port of the OP2 -->

## Scope

An application of a lambda, whose variable wire runs from the lambda to its use in the body:

```wl
THeapDiagram[TLam[x, x + 1][TNum[41]]]
```
<!-- => a string diagram: LAM, OP2 + and APP boxes, a var wire from the LAM to the OP2, the numbers 1 and 41, and the open output port of the APP -->

---

The identity function wires its variable straight back to itself:

```wl
THeapDiagram[TLam[x, x]]
```
<!-- => a string diagram: one LAM box with a loop on its var wire and one open output port -->

---

Draw two terms in one diagram:

```wl
THeapDiagram[{TSup[1, 2], TLam[x, x]}]
```
<!-- => a string diagram with a SUP box fed by 1 and 2 and, apart from it, the identity LAM -->

---

The two projections of a duplication leave through two ports:

```wl
THeapDiagram[Plus @@ TDup[TSup[1, 2]]]
```
<!-- => a string diagram: 1 and 2 wired into a SUP, the SUP into a DUP, and two dup wires from the DUP into an OP2 + -->

---

Start from an empty heap:

```wl
TReset[];
```

Build a term without keeping it:

```wl
TSup[1, 2] + 3
```
<!-- => a TTerm summary box with tag OP2 -->

Without a seed the diagram shows the agents that heap cells refer to, which leaves out the addition held only by the Wolfram Language:

```wl
THeapDiagram[]
```
<!-- => a string diagram: the numbers 1 and 2 wired into a SUP box with one open output wire -->

## Properties and Relations

<code>[THeapGraph]()</code> draws the same agents as a [Graph]():

```wl
THeapGraph[TSup[1, 2] + 3, ImageSize -> 300, AspectRatio -> 1]
```
<!-- => a Graph of 5 vertices: the numbers 1 and 2 feed the SUP, which feeds the OP2 + together with 3 -->

---

<code>[Term]()</code> gives the same term as a nested expression:

```wl
Term[TSup[1, 2] + 3]
```
<!-- => Term["OP2", "+", Term["SUP", L, Term["NUM", 1], Term["NUM", 2]], Term["NUM", 3]] -->
