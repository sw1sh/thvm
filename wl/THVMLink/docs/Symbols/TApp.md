---
Template: Symbol
Name: TApp
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TApp
Keywords: [apply, application, lambda, beta, kernel]
SeeAlso: [TLam, TJit, TUOpMul, TRealize, TKernelCount]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TApp]()[*fun*, *arg*]</code> constructs the application of *fun* to *arg*.

## Details & Options

- When *fun* is a <code>[TLam]()</code> whose body is a UOp graph and *arg* carries a shape, the application JIT-materializes the body into a `UOP_KERNEL` with the bound variable as a symbolic input slot: compile once, dispatch per subsequent argument.
- Lazy: realize the application with <code>[TRealize]()</code> to fire the kernel and read the result.
- A first realize appends the materialized kernel to the side table; see <code>[TKernelCount]()</code>.

## Basic Examples

Apply a squaring lambda to a tensor:

```wl
sq = TLam[w, TUOpMul[w, w]];
Normal @ TRealize @ TApp[sq, TTensorCreate[{1., 2., 3., 4.}]]
```
<!-- => {1., 4., 9., 16.} -->

## Properties and Relations

Realizing an application emits its kernel, which a subsequent <code>[TKernelSource]()</code> can inspect via <code>[TKernelCount]()[] - 1</code>:

```wl
TRealize @ TApp[TLam[w, TUOpMul[w, w]], TTensorCreate[Range[1., 8.]]];
StringContainsQ[TKernelSource[TKernelCount[] - 1, "C"], "void k("]
```
<!-- => True -->
