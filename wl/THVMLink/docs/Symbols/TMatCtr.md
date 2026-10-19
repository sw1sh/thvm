---
Template: Symbol
Name: TMatCtr
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TMatCtr
Keywords: [match, constructor, destructure, pattern, fields]
SeeAlso: [TMatNum, TMatChain, TCtr, TIfZero]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TMatCtr]()[*label*, *handler*, *fallback*]</code> builds a matcher that applies *handler* to the children of a constructor with the integer *label* and applies *fallback* to any other argument.

<!-- #| annotation: 26.10.19: TMatCtr is an alias, defined in Switch.wl as TMatNum[label, handler, fallback]; the name records the intent of destructuring a CTR, which the APP-MAT-CTR rule does by applying the handler to the fields through a chain of APPs (HVM4's APP-MAT-CTR-MAT layout). Lazy.wl uses it to destructure its lazy cons cells. A handler with fewer variables than the constructor has fields is left applied to the extra fields; the handler and fallback go through ttermRaw unlifted, so numbers there are written TNum[n]. -->

## Details & Options

- [TMatCtr]() builds the same term as <code>[TMatNum]()[*label*, *handler*, *fallback*]</code>.
- Applied to a constructor with label *label*, the matcher applies *handler* to its children in order, so *handler* is a curried function with one variable per child, such as <code>[TLam]()[*a*, [TLam]()[*b*, *body*]]</code>. A constructor without children gives *handler* itself.
- Applied to a number equal to *label*, the matcher gives *handler* itself, and applied to any other argument it gives *fallback* applied to the argument.
- A superposed argument is matched branch by branch.
- An anonymous constructor uses the label 0.
- For several labels, <code>[TMatChain]()</code> nests one matcher per label.

## Basic Examples

A matcher that adds the two fields of a constructor with label 1:

```wl
m = TMatCtr[1, TLam[a, TLam[b, a + b]], TLam[k, TNum[0]]]
```
<!-- => a TTerm summary box with tag MAT and ext 1 -->

Apply it to such a constructor:

```wl
TWnf[TApp[m, TCtr[1, TNum[10], TNum[20]]]]
```
<!-- => a TNum summary box showing the number 30 -->

## Scope

A constructor with another label goes to the fallback, here the constant 0:

```wl
TTermVal[TWnf[TApp[TMatCtr[1, TLam[a, TLam[b, a + b]], TLam[k, TNum[0]]], TCtr[2, TNum[10], TNum[20]]]]]
```
<!-- => 0 -->

---

The fallback receives the argument:

```wl
Term[TWnf[TApp[TMatCtr[1, TLam[a, a], TLam[k, k]], TCtr[2, TNum[9]]]]]
```
<!-- => Term["CTR", 2, Term["NUM", 9]] -->

---

A constructor with one child is matched by a handler of one variable:

```wl
TTermVal[TWnf[TApp[TMatCtr[2, TLam[a, a * 2], TLam[k, TNum[0]]], TCtr[2, TNum[21]]]]]
```
<!-- => 42 -->

---

A constructor without children gives the handler itself:

```wl
TTermVal[TWnf[TApp[TMatCtr[0, TNum[7], TLam[k, TNum[0]]], TCtr[0]]]]
```
<!-- => 7 -->

---

The handler can build a new constructor from the fields, here swapping them:

```wl
Term[TWnf[TApp[TMatCtr[1, TLam[p, TLam[q, TCtr[1, q, p]]], TLam[k, TNum[0]]], TCtr[1, TNum[10], TNum[20]]]]]
```
<!-- => Term["CTR", 1, Term["NUM", 20], Term["NUM", 10]] -->

---

A superposition of constructors is matched branch by branch:

```wl
TTermVal /@ TCollapse[TApp[TMatCtr[1, TLam[a, TLam[b, a + b]], TLam[k, TNum[0]]], TSup[TCtr[1, TNum[1], TNum[2]], TCtr[1, TNum[10], TNum[20]]]]]
```
<!-- => {3, 30} -->

## Properties and Relations

[TMatCtr]() builds the same matcher as <code>[TMatNum]()</code>:

```wl
Term[TMatCtr[1, TNum[5], TNum[6]]] === Term[TMatNum[1, TNum[5], TNum[6]]]
```
<!-- => True -->

---

<code>[TMatChain]()</code> matches several labels, each with its own handler:

```wl
TTermVal[TWnf[TApp[TMatChain[<|1 -> TLam[a, TLam[b, a + b]], 2 -> TLam[a, 2 a]|>, TLam[k, TNum[0]]], TCtr[2, TNum[21]]]]]
```
<!-- => 42 -->

## Possible Issues

A number equal to the label gives the handler itself, without applying it:

```wl
Term[TWnf[TApp[TMatCtr[1, TLam[a, TLam[b, a + b]], TLam[k, TNum[0]]], TNum[1]]]]
```
<!-- => Term["LAM", ..., Term["LAM", ..., Term["OP2", "+", Term["VAR", ...], Term["VAR", ...]]]] -->

---

A handler with fewer variables than the constructor has children leaves the remaining children applied:

```wl
Term[TWnf[TApp[TMatCtr[1, TLam[a, a], TLam[k, TNum[0]]], TCtr[1, TNum[10], TNum[20]]]]]
```
<!-- => Term["APP", Term["NUM", 10], Term["NUM", 20]] -->
