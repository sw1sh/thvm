---
Template: Symbol
Name: TMatChain
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TMatChain
Keywords: [match, dispatch, case analysis, constructor, label, algebraic data type]
SeeAlso: [TMatNum, TMatCtr, TCtr, TIfZero, TSup]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TMatChain]()[<|$l_1$ -> $h_1$, $l_2$ -> $h_2$, …|>, *fallback*]</code> builds a matcher that uses the handler $h_i$ for an argument with the integer label $l_i$ and applies *fallback* to any other argument.

<!-- #| annotation: 26.10.19: TMatChain is pure WL sugar in Switch.wl: a Fold over the reversed rules that nests TMatNum[l, h, rest], so the first key is the outermost MAT and an empty association gives the fallback term itself. Each MAT dispatches on the argument's tag and ext through APP-MAT (NUM equal -> handler as is, CTR with that ext -> handler applied to the fields through an APP chain, otherwise the next arm, finally APP fallback arg via MAT-MIS) and commutes over a SUP argument through APP-MAT-SUP. Like TMatNum it passes handlers through ttermRaw, so a bare Integer handler is a raw word. This mirrors HVM4's λ{#L1: h1; #L2: h2; ...} case lambdas. -->

## Details & Options

- [TMatChain]() nests one matcher per rule, <code>[TMatNum]()[$l_1$, $h_1$, [TMatNum]()[$l_2$, $h_2$, …, *fallback*]]</code>, so the labels are tested in the order of the association.
- Applied to a constructor whose label is $l_i$, the matcher applies $h_i$ to the children of the constructor in order, so $h_i$ is a curried function with one variable per child.
- Applied to a number equal to $l_i$, the matcher gives $h_i$ itself.
- Applied to any other argument, the matcher gives *fallback* applied to the argument.
- A superposed argument is matched branch by branch.
- The labels are integers, and the handlers and the fallback are terms; a number is written <code>[TNum]()[*n*]</code>.
- An empty association gives *fallback* itself.

## Basic Examples

A matcher with an answer for each of the numbers 0, 1 and 2:

```wl
mc = TMatChain[<|0 -> TNum[10], 1 -> TNum[20], 2 -> TNum[30]|>, TLam[k, TNum[99]]]
```
<!-- => a TTerm summary box with tag MAT and ext 0 -->

Apply it to 1:

```wl
TWnf[TApp[mc, TNum[1]]]
```
<!-- => a TNum summary box showing the number 20 -->

## Scope

A number without a rule goes to the fallback:

```wl
TTermVal[TWnf[TApp[TMatChain[<|0 -> TNum[10], 1 -> TNum[20]|>, TLam[k, TNum[99]]], TNum[5]]]]
```
<!-- => 99 -->

---

The fallback receives the argument, here adding 100 to it:

```wl
TTermVal[TWnf[TApp[TMatChain[<|0 -> TNum[1]|>, TLam[k, k + 100]], TNum[5]]]]
```
<!-- => 105 -->

---

A constructor with label 1 has its two fields added:

```wl
TTermVal[TWnf[TApp[TMatChain[<|1 -> TLam[a, TLam[b, a + b]], 2 -> TLam[a, 2 a]|>, TLam[k, TNum[0]]], TCtr[1, TNum[3], TNum[4]]]]]
```
<!-- => 7 -->

---

A constructor with label 2 has its one field doubled:

```wl
TTermVal[TWnf[TApp[TMatChain[<|1 -> TLam[a, TLam[b, a + b]], 2 -> TLam[a, 2 a]|>, TLam[k, TNum[0]]], TCtr[2, TNum[21]]]]]
```
<!-- => 42 -->

---

A superposition of arguments is matched branch by branch:

```wl
TTermVal /@ TCollapse[TApp[TMatChain[<|0 -> TNum[10], 1 -> TNum[20], 2 -> TNum[30]|>, TLam[k, TNum[99]]], TSup[0, 2]]]
```
<!-- => {10, 30} -->

---

With no rules the matcher is the fallback:

```wl
Term[TMatChain[<||>, TLam[k, TNum[99]]]]
```
<!-- => Term["LAM", id, Term["NUM", 99]] -->

## Applications

Compute areas of a small shape type, where label 0 is a square with its side and label 1 a rectangle with its width and height:

```wl
TTermVal /@ TCollapse[TApp[TMatChain[<|0 -> TLam[s, s * s], 1 -> TLam[w, TLam[h, w * h]]|>, TLam[k, TNum[0]]], TSup[TCtr[0, TNum[3]], TCtr[1, TNum[2], TNum[5]]]]]
```
<!-- => {9, 10} -->

## Properties and Relations

The chain is a nest of <code>[TMatNum]()</code> matchers, the first rule outermost:

```wl
Term[TMatChain[<|0 -> TNum[10], 1 -> TNum[20]|>, TLam[k, TNum[99]]]]
```
<!-- => Term["MAT", 0, Term["NUM", 10], Term["MAT", 1, Term["NUM", 20], Term["LAM", id, Term["NUM", 99]]]] -->

---

The same nest written by hand gives the same result:

```wl
TTermVal[TWnf[TApp[TMatNum[0, TNum[10], TMatNum[1, TNum[20], TLam[k, TNum[99]]]], TNum[1]]]]
```
<!-- => 20 -->

---

With a single rule for 0, the chain is the conditional of <code>[TIfZero]()</code>:

```wl
TTermVal[TWnf[TApp[TMatChain[<|0 -> TNum[10]|>, TLam[k, TNum[20]]], TNum[0]]]]
```
<!-- => 10 -->

## Possible Issues

A number equal to a label gives the handler itself, without applying it:

```wl
Term[TWnf[TApp[TMatChain[<|1 -> TLam[a, TLam[b, a + b]]|>, TLam[k, TNum[0]]], TNum[1]]]]
```
<!-- => Term["LAM", ..., Term["LAM", ..., Term["OP2", "+", Term["VAR", ...], Term["VAR", ...]]]] -->
