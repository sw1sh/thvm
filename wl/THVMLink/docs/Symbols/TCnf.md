---
Template: Symbol
Name: TCnf
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TCnf
Keywords: [collapsed normal form, superposition, readback, normal form, interaction combinator]
SeeAlso: [TWnf, TNf, TCollapse, TSup, TDup]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TCnf]()[*t*]</code> reduces the term *t* to collapsed normal form, in which a superposition inside the term is lifted to the top.

<!-- #| annotation: 26.10.19: TCnf wraps cnf_at (src/cnf/_.c), a port of HVM4's clang/cnf readback. It is wnf plus a recursive walk that lifts the first SUP child of a node, NODE(..., SUP^L(a, b), ...) -> SUP^L(NODE(..., a, ...), NODE(..., b, ...)), cloning the other children through fresh DUPs under the same label, and it is the layer where plain DP projections are actually duplicated (they are Levy-opaque under wnf). TCollapse runs cnf at every step of its walk and TNf finishes with a cnf of the root. A bare Integer argument is not lifted to a NUM here. -->

## Details & Options

- Collapsed normal form (CNF) reduces *t* to weak head normal form and then walks the children of each node, reducing them in turn, so it also reduces under a lambda.
- A node with a superposition <code>[TSup]()[*L*, *a*, *b*]</code> among its children is rewritten to a superposition with label *L* of two copies of the node, one with *a* and one with *b*. The other children are shared between the copies through duplications with the same label.
- Only the first superposition found is lifted; one that appears later stays inside the branches, for <code>[TCollapse]()</code> or a later [TCnf]() to lift.
- A duplication projection is resolved against the collapsed form of its body, so a copied constructor or lambda comes out without projection nodes.
- The result is a [TTerm](). A term with no superposition and no projection comes back as it is.
- CNF sits between weak head normal form, <code>[TWnf]()</code>, which stops at the head, and full normal form, <code>[TNf]()</code>, which also fires every redex left anywhere on the heap.

## Basic Examples

Collapse a constructor that holds a superposition:

```wl
r = TCnf[TCtr[1, TSup[1, 2], TNum[3]]]
```
<!-- => a TTerm summary box with tag SUP -->

The superposition now sits on top, over two copies of the constructor:

```wl
Term[r]
```
<!-- => Term["SUP", L, Term["CTR", 1, Term["NUM", 1], Term["DP0", L, Term["NUM", 3]]], Term["CTR", 1, Term["NUM", 2], Term["DP1", L, Term["NUM", 3]]]], with L the label of the superposition -->

## Scope

Collapsed normal form reduces the body of a lambda:

```wl
Term[TCnf[TLam[x, TNum[1] + 2]]]
```
<!-- => Term["LAM", id, Term["NUM", 3]], with id the binder's heap location -->

---

A superposition inside a lambda body is lifted out of the lambda:

```wl
Term[TCnf[TLam[x, TCtr[1, TSup[1, 2]]]]]
```
<!-- => Term["SUP", L, Term["LAM", ..., Term["CTR", 1, Term["NUM", 1]]], Term["LAM", ..., Term["CTR", 1, Term["NUM", 2]]]] -->

---

A projection of a duplicated constructor comes out as a plain constructor:

```wl
Term[TCnf[First[TDup[TCtr[2, TNum[1]]]]]]
```
<!-- => Term["CTR", 2, Term["NUM", 1]] -->

---

Arithmetic on numbers reduces to a number:

```wl
Term[TCnf[TNum[1] + 2]]
```
<!-- => Term["NUM", 3] -->

---

A term with no superposition comes back as it is:

```wl
Term[TCnf[TCtr[1, TNum[2], TNum[3]]]]
```
<!-- => Term["CTR", 1, Term["NUM", 2], Term["NUM", 3]] -->

## Properties and Relations

<code>[TWnf]()</code> stops at the constructor head and leaves the superposition inside:

```wl
Term[TWnf[TCtr[1, TSup[1, 2], TNum[3]]]]
```
<!-- => Term["CTR", 1, Term["SUP", L, Term["NUM", 1], Term["NUM", 2]], Term["NUM", 3]] -->

---

<code>[TWnf]()</code> also leaves the projection inside a copied constructor:

```wl
Term[TWnf[First[TDup[TCtr[2, TNum[1]]]]]]
```
<!-- => Term["CTR", 2, Term["DP0", L, Term["NUM", 1]]] -->

---

<code>[TCollapse]()</code> repeats the lifting down every branch and gives the leaves:

```wl
Term /@ TCollapse[TCtr[1, TSup[1, 2], TNum[3]]]
```
<!-- => {Term["CTR", 1, Term["NUM", 1], Term["NUM", 3]], Term["CTR", 1, Term["NUM", 2], Term["NUM", 3]]} -->

## Possible Issues

Only the first superposition is lifted, and a second one stays inside each branch:

```wl
Term[TCnf[TCtr[1, TSup[1, 2], TSup[3, 4]]]]
```
<!-- => Term["SUP", L, Term["CTR", 1, Term["NUM", 1], Term["DP0", L, Term["SUP", M, Term["NUM", 3], Term["NUM", 4]]]], Term["CTR", 1, Term["NUM", 2], Term["DP1", L, Term["SUP", M, ...]]]] -->

---

<code>[TCollapse]()</code> lifts the second superposition as well and gives all four combinations:

```wl
Term /@ TCollapse[TCtr[1, TSup[1, 2], TSup[3, 4]]]
```
<!-- => {Term["CTR", 1, Term["NUM", 1], Term["NUM", 3]], Term["CTR", 1, Term["NUM", 1], Term["NUM", 4]], Term["CTR", 1, Term["NUM", 2], Term["NUM", 3]], Term["CTR", 1, Term["NUM", 2], Term["NUM", 4]]} -->
