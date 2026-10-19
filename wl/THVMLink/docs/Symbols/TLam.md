---
Template: Symbol
Name: TLam
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TLam
Keywords: [lambda, function, binder, variable, interaction combinator, kernel]
SeeAlso: [TApp, TDup, TWnf, TCnf, TRealize, TUOpMul]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TLam]()[*x*, *body*]</code> builds a lambda whose variable is the symbol *x* and whose body is *body*.

<code>[TLam]()[*body*]</code> builds a lambda that ignores its argument and gives *body*.

<!-- #| annotation: 26.10.19: TLam is HoldAll. It allocates the one body cell first, then evaluates Function[x, body][TVarFor[loc]], so x is replaced by a VAR term pointing at the lambda before the body evaluates; that is what lets the integer arithmetic UpValues fire on x (a plain VAR is a numeric term), and why a tensor body must use TUOp* constructors, since x * x over a plain VAR builds an OP2 rather than a UOp. TLamShape[shape, x, body] (no page yet) records a shape for the variable, after which the sugar builds UOps. A variable used more than once gets an automatic DUP chain with labels from 0x10000 (src/lam/auto_dup.c). The APP-LAM interaction materializes a UOp body into a UOP_KERNEL on first tensor application (src/interact/app_lam.c). The one-argument form writes body into the cell with THeapSet, so a bare Integer there is a raw word. -->

## Details & Options

- [TLam]() holds its arguments. In *body*, *x* stands for the variable of the lambda, so the body can be built from term constructors and the arithmetic of terms, as in <code>[TLam]()[*x*, *x* + 1]</code>.
- The result is a [TTerm]() with tag `LAM`. It is applied with <code>[TApp]()</code>, or by giving it an argument as in *lam*[*arg*].
- A variable used more than once is duplicated automatically, so each use gets its own copy of the argument.
- A lambda is in weak head normal form, so <code>[TWnf]()</code> leaves it as it is, while <code>[TCnf]()</code> reduces its body.
- When the body is a graph of UOp (micro-operation) nodes, such as <code>[TUOpMul]()[*w*, *w*]</code>, and the argument is a tensor, the first application compiles the body into a kernel whose input is the variable, and <code>[TRealize]()</code> runs it.
- Over a tensor variable, the body is written with UOp constructors: the arithmetic of terms on a variable builds integer arithmetic.
- Applying a lambda consumes it; a lambda applied more than once is shared through <code>[TDup]()</code>.

## Basic Examples

The successor function:

```wl
inc = TLam[x, x + 1]
```
<!-- => a TTerm summary box with tag LAM -->

Its canonical form shows the variable in the body:

```wl
Term[inc]
```
<!-- => Term["LAM", id, Term["OP2", "+", Term["VAR", id], Term["NUM", 1]]], with id the lambda's heap location -->

Apply it to 41 and reduce:

```wl
TWnf[TApp[inc, TNum[41]]]
```
<!-- => a TNum summary box showing the number 42 -->

## Scope

A lambda that ignores its argument:

```wl
TTermVal[TWnf[TApp[TLam[TNum[5]], TNum[9]]]]
```
<!-- => 5 -->

---

A variable that does not occur in the body discards the argument:

```wl
Term[TWnf[TLam[x, TNum[7]][TNum[4]]]]
```
<!-- => Term["NUM", 7] -->

---

Nested lambdas take their arguments one at a time:

```wl
TTermVal[TWnf[TLam[a, TLam[b, a - b]][TNum[10]][TNum[3]]]]
```
<!-- => 7 -->

---

A lambda can take a function as its argument, here applying it twice:

```wl
TTermVal[TWnf[TLam[f, TLam[y, f[f[y]]]][TLam[z, z * 2]][TNum[5]]]]
```
<!-- => 20 -->

---

A variable used twice is duplicated automatically:

```wl
Term[TLam[x, x * x]]
```
<!-- => Term["LAM", id, Term["OP2", "*", Term["DP0", L, Term["VAR", id]], Term["DP1", L, Term["VAR", id]]]], with L at least 65536 -->

---

A body of UOp nodes compiles to a kernel when the lambda is applied to a tensor:

```wl
Normal[TRealize[TApp[TLam[w, TUOpMul[w, w]], TTensorCreate[{1., 2., 3., 4.}]]]]
```
<!-- => {1., 4., 9., 16.} -->

---

The body may reduce the tensor, here to the sum of its squares:

```wl
Normal[TRealize[TApp[TLam[w, TUOpReduce[TUOpMul[w, w], 0, "SUM"]], TTensorCreate[{1., 2., 3.}]]]]
```
<!-- => {14.} -->

## Properties and Relations

A superposed argument takes the same branch in every use of the variable:

```wl
TTermVal /@ TCollapse[TLam[x, x * x][TSup[5, 7]]]
```
<!-- => {25, 49} -->

---

A superposition of lambdas applies each of them:

```wl
TTermVal /@ TCollapse[TApp[TSup[TLam[x, x + 1], TLam[x, 2 x]], 10]]
```
<!-- => {11, 20} -->

---

<code>[TCnf]()</code> reduces the body of a lambda, which <code>[TWnf]()</code> leaves alone:

```wl
Term[TCnf[TLam[x, TNum[1] + 2]]]
```
<!-- => Term["LAM", id, Term["NUM", 3]] -->

---

The lambda that keeps its first argument and discards its second:

```wl
TTermVal[TWnf[TLam[a, TLam[b, a]][TNum[1]][TNum[2]]]]
```
<!-- => 1 -->

## Possible Issues

The arithmetic of terms on the variable builds integer arithmetic, so over a tensor it does not realize to a tensor:

```wl
TRealize[TApp[TLam[w, w * w], TTensorCreate[{1., 2., 3.}]]]
```
<!-- => a TTerm summary box with tag OP2, not a TTensor -->

---

The variable must be a symbol, and other forms return unevaluated:

```wl
TLam[1, 2]
```
<!-- => TLam[1, 2] -->
