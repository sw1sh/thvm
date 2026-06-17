---
Template: Symbol
Name: TRealize
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TRealize
Keywords: [realize, materialize, dispatch, kernel, fire]
SeeAlso: [TMaterialize, TTensorData, TTensorShape, TUOpMul, TGrad, TKernelCount]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TRealize]()[*expr*]</code> fires the whole pipeline for a UOp graph: it schedules and kernelizes *expr* (the <code>[TMaterialize]()</code> step), then beta-reduces and dispatches the resulting kernels, returning a realized `TTensor`.

## Details & Options

- <code>[TRealize]()[*expr*]</code> is <code>[TWnf]()[[TMaterialize]()[*expr*]]</code>: the materialize pass rewrites UOPs to `UOP_KERNEL`s in place, then weak-head normalization dispatches them.
- Nothing is computed until [TRealize]() runs; UOp constructors such as <code>[TUOpMul]()</code> only build heap nodes.
- The realized result reads back with <code>[Normal]()</code>, <code>[TTensorData]()</code>, <code>[TTensorShape]()</code>, and <code>[TTensorDType]()</code>.
- Each kernel is clang-compiled on the CPU backend at dispatch time, so the first realize of a new graph pays a one-time compile cost; <code>[TJit]()</code> caches the dispatch sequence to amortize repeats.

## Basic Examples

Realize an elementwise product and read its buffer:

```wl
x = TTensorCreate[{1., 2., 3., 4.}];
Normal @ TRealize @ (x*x)
```
<!-- => {1., 4., 9., 16.} -->

## Scope

Realize a composed graph in one shot; the intermediate reductions never leave the runtime:

```wl
x = TTensorCreate[{1., 2., 3., 4.}];
Normal @ TRealize @ Total[x^2]
```
<!-- => {30.} -->

## Properties and Relations

Realizing a graph appends kernels to the side table, so <code>[TKernelCount]()</code> increases:

```wl
before = TKernelCount[];
TRealize @ (TTensorCreate[{1., 2.}] * TTensorCreate[{3., 4.}]);
TKernelCount[] > before
```
<!-- => True -->
