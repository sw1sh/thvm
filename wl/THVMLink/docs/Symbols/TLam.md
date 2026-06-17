---
Template: Symbol
Name: TLam
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TLam
Keywords: [lambda, binder, function, JIT, kernel]
SeeAlso: [TApp, TJit, TUOpMul, TRealize, TKernelCount]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TLam]()[*x*, *body*]</code> constructs a lambda whose binder is the symbol *x* and whose *body* is a UOp expression referring to it.

## Details & Options

- [HoldAll](): *x* is the binder symbol and *body* is held so it can refer to it, e.g. <code>[TLam]()[*w*, [TUOpMul]()[*w*, *w*]]</code>.
- When *body* is a UOp graph and the first <code>[TApp]()</code> argument carries a shape, the application JIT-materializes *body* into a `UOP_KERNEL` with the bound variable as a symbolic input slot, compiling once and dispatching with each subsequent argument.
- Bodies that are not UOp graphs (curried lambdas, control flow) skip that JIT step.
- Apply it with <code>[TApp]()</code>.

## Basic Examples

A squaring lambda is a `TTerm`:

```wl
MatchQ[TLam[w, TUOpMul[w, w]], _TTerm]
```
<!-- => True -->

## Scope

Apply the lambda to a tensor and realize the result:

```wl
sq = TLam[w, TUOpMul[w, w]];
Normal @ TRealize @ TApp[sq, TTensorCreate[{1., 2., 3., 4.}]]
```
<!-- => {1., 4., 9., 16.} -->
