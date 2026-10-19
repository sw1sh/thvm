---
Template: Symbol
Name: TItrs
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TItrs
Keywords: [interactions, counter, cost, reduction, profiling]
SeeAlso: [TStep, TWnf, TInteract, TCausalGraph, TReset]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TItrs]()[]</code> gives the number of interactions fired since the runtime was last reset or initialized.

<!-- #| annotation: 26.10.19: TItrs reads the C ITRS counter, which every interaction rule bumps once (APP-LAM, DUP-*, OP2-*, MAT dispatch, DSU/DDU, EQL, the UOp rules); SUP descent and other walker plumbing do not count. TReset and TInit zero it. The trace layer numbers events from the same counter, so TMultiTrace event ids continue across a page unless it resets first. Prior art: HVM4's interaction count, the usual cost measure of interaction-net reducers. -->

## Details & Options

- One interaction is one rewrite of the interaction net: a beta reduction, one step of a duplication, an arithmetic operation, a matcher dispatch and so on.
- Building a term fires no interactions, and neither does reading it back with <code>[Term]()</code>.
- <code>[TReset]()</code> and <code>[TInit]()</code> set the count to 0.
- The difference between two counts is the cost of the reduction between them, which makes the count a measure of work that does not depend on timing.
- The events of <code>[TCausalGraph]()</code> and <code>[TMultiTrace]()</code> are the same interactions, so a causal graph has one vertex for each interaction counted.

## Basic Examples

Start from a reset runtime, so the count is at 0:

```wl
TReset[];
```

Apply the successor function to 41:

```wl
TTermVal[TWnf[TLam[x, x + 1][TNum[41]]]]
```
<!-- => 42 -->

The beta reduction and the addition are two interactions:

```wl
TItrs[]
```
<!-- => 2 -->

## Scope

Start from a reset runtime:

```wl
TReset[];
```

Build an application without reducing it:

```wl
TLam[x, x + 1][TNum[41]]
```
<!-- => a TTerm summary box with tag APP -->

Building fires nothing:

```wl
TItrs[]
```
<!-- => 0 -->

---

Start from a reset runtime:

```wl
TReset[];
```

<code>[TStep]()</code> fires a single interaction:

```wl
Term[TStep[TLam[x, x + 1][TNum[41]]]]
```
<!-- => Term["OP2", "+", Term["NUM", 41], Term["NUM", 1]] -->

The count shows it:

```wl
TItrs[]
```
<!-- => 1 -->

---

Start from a reset runtime:

```wl
TReset[];
```

Add the numbers from 1 to 10 one at a time:

```wl
TTermVal[TWnf[Fold[#1 + #2 &, TNum[0], TNum /@ Range[10]]]]
```
<!-- => 55 -->

Each addition is one interaction:

```wl
TItrs[]
```
<!-- => 10 -->

---

Start from a reset runtime:

```wl
TReset[];
```

Apply the doubling function three times to 1:

```wl
TTermVal[TWnf[TLam[f, TLam[y, f[f[f[y]]]]][TLam[z, z * 2]][TNum[1]]]]
```
<!-- => 8 -->

Sharing the function among its three uses takes duplications as well as beta reductions and multiplications:

```wl
TItrs[]
```
<!-- => 14 -->

## Properties and Relations

Start from a reset runtime:

```wl
TReset[];
```

Collapse the square of a superposition:

```wl
TTermVal /@ TCollapse[TLam[x, x * x][TSup[5, 7]]]
```
<!-- => {25, 49} -->

The collapse took eight interactions:

```wl
TItrs[]
```
<!-- => 8 -->

---

The causal graph of the same reduction has one vertex for each of them:

```wl
VertexCount[TCausalGraph[TLam[x, x * x][TSup[5, 7]]]]
```
<!-- => 8 -->

---

Start from a reset runtime:

```wl
TReset[];
```

Reading a term back walks the heap without firing anything:

```wl
Term[TSup[1, 2] + 3]
```
<!-- => Term["OP2", "+", Term["SUP", 1, Term["NUM", 1], Term["NUM", 2]], Term["NUM", 3]] -->

The count is still 0:

```wl
TItrs[]
```
<!-- => 0 -->
