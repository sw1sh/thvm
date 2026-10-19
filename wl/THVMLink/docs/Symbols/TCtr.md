---
Template: Symbol
Name: TCtr
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TCtr
Keywords: [constructor, algebraic data type, record, interaction combinator]
SeeAlso: [TMatCtr, TMatChain, TMatNum, TBookCtr, TNum, Term]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TCtr]()[*label*, $c_1$, $c_2$, …]</code> builds a constructor term with the integer *label* and the children $c_i$.

<!-- #| annotation: 26.10.19: TCtr wraps thvm_wl_term_new_ctr, which lays the constructor out in the dynamic heap as its child count (a NUM) followed by the children, with the label in the term's ext field. The arity cap of 16 is REWRITE_MAX_ARITY of src/thvm.h, which matches HVM4's CTR limit. -->

## Details & Options

- A constructor is a tagged record: *label* names the constructor and the children are its fields. The result is a [TTerm]() with tag `CTR` whose extension field is *label*.
- A constructor has at most 16 children, and it may have none. More children issue the message `TCtr::arity`, and the input returns unevaluated.
- The children are terms. A bare [Integer]() child is read as <code>[TNum]()[*n*]</code>.
- A constructor is already in weak head normal form, so <code>[TWnf]()</code> leaves it unchanged.
- <code>[TMatCtr]()</code> and <code>[TMatChain]()</code> select a handler by the label of a constructor and apply it to the children in order.
- Duplicating a constructor copies it together with its children, and a superposition among the children is lifted to the top by <code>[TCnf]()</code> and <code>[TCollapse]()</code>.
- The constructor is built in the dynamic heap, where <code>[Term]()</code> and the reducer read it; <code>[TBookCtr]()</code> builds one in the book heap instead.

## Basic Examples

A two-field constructor with label 3:

```wl
c = TCtr[3, TNum[10], TNum[20]]
```
<!-- => a TTerm summary box with tag CTR and ext 3 -->

Its canonical form shows the label and the two fields:

```wl
Term[c]
```
<!-- => Term["CTR", 3, Term["NUM", 10], Term["NUM", 20]] -->

## Scope

A constructor may have no children:

```wl
Term[TCtr[0]]
```
<!-- => Term["CTR", 0] -->

---

Bare integer children are read as numbers:

```wl
Term[TCtr[1, 5, 6]]
```
<!-- => Term["CTR", 1, Term["NUM", 5], Term["NUM", 6]] -->

---

A child may itself be a constructor:

```wl
Term[TCtr[1, TCtr[2, TNum[5]], TNum[7]]]
```
<!-- => Term["CTR", 1, Term["CTR", 2, Term["NUM", 5]], Term["NUM", 7]] -->

---

A child may be any term, such as a lambda:

```wl
Term[TCtr[1, TLam[x, x + 1]]]
```
<!-- => Term["CTR", 1, Term["LAM", id, Term["OP2", "+", Term["VAR", id], Term["NUM", 1]]]] -->

---

Reduction leaves a constructor as it is:

```wl
Term[TWnf[TCtr[3, TNum[10], TNum[20]]]]
```
<!-- => Term["CTR", 3, Term["NUM", 10], Term["NUM", 20]] -->

---

A matcher for label 1 adds the two fields of a constructor with that label:

```wl
TTermVal[TWnf[TApp[TMatCtr[1, TLam[a, TLam[b, a + b]], TLam[k, TNum[0]]], TCtr[1, TNum[10], TNum[20]]]]]
```
<!-- => 30 -->

---

A chain of matchers dispatches on several labels, here doubling the field of a label 2 constructor:

```wl
TTermVal[TWnf[TApp[TMatChain[<|1 -> TLam[a, TLam[b, a + b]], 2 -> TLam[a, 2 a]|>, TLam[k, TNum[0]]], TCtr[2, TNum[21]]]]]
```
<!-- => 42 -->

## Properties and Relations

A superposition among the children collapses to one constructor per branch:

```wl
Term /@ TCollapse[TCtr[1, TSup[1, 2], TNum[3]]]
```
<!-- => {Term["CTR", 1, Term["NUM", 1], Term["NUM", 3]], Term["CTR", 1, Term["NUM", 2], Term["NUM", 3]]} -->

---

Duplicating a constructor gives two copies of it:

```wl
Term /@ TCnf /@ TDup[TCtr[4, TNum[1], TNum[2]]]
```
<!-- => {Term["CTR", 4, Term["NUM", 1], Term["NUM", 2]], Term["CTR", 4, Term["NUM", 1], Term["NUM", 2]]} -->

---

In the heap a constructor occupies its child count followed by its children:

```wl
c = TCtr[3, TNum[10], TNum[20]]
```
<!-- => a TTerm summary box with tag CTR and ext 3 -->

Read the cells with <code>[THeapRead]()</code>:

```wl
TTermVal /@ THeapRead /@ (TTermVal[c] + Range[0, 2])
```
<!-- => {2, 10, 20} -->

## Possible Issues

A constructor whose label has no matcher goes to the fallback, here the constant 0:

```wl
TTermVal[TWnf[TApp[TMatCtr[1, TLam[a, TLam[b, a + b]], TLam[k, TNum[0]]], TCtr[2, TNum[10], TNum[20]]]]]
```
<!-- => 0 -->

---

A constructor takes at most 16 children:

```wl
TCtr[1, Sequence @@ Range[17]]
```
<!-- => the message TCtr::arity is issued and TCtr[1, 1, 2, ..., 17] returns unevaluated -->
