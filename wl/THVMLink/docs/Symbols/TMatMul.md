---
Template: Symbol
Name: TMatMul
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TMatMul
Keywords: [matmul, GEMM, BLAS, linear algebra, reduce]
SeeAlso: [TUOpMul, TUOpReduce, TUOpReshape, TGlorot, TRealize]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TMatMul]()[*A*, *B*]</code> computes the matrix product *A* . *B*, where *A* has shape `{M, K}`, *B* has shape `{K, N}`, and the result has shape `{M, N}`.

## Details & Options

- Lowered as `RESHAPE` + `EXPAND` to a common `{M, K, N}` shape, an elementwise <code>[TUOpMul]()</code>, then a `"SUM"` <code>[TUOpReduce]()</code> along axis 1.
- On the CPU backend `cpu_blas_dispatch` recognizes this pattern and routes it to `cblas_sgemm`.
- Both inputs must be rank-2 with matching inner dimension `K`; realize the result with <code>[TRealize]()</code>.

## Basic Examples

Multiply two 2x2 matrices:

```wl
A = TTensorCreate[{{1., 2.}, {3., 4.}}];
B = TTensorCreate[{{5., 6.}, {7., 8.}}];
Normal @ TRealize @ TMatMul[A, B]
```
<!-- => {{19., 22.}, {43., 50.}} -->

## Properties and Relations

The result shape is `{M, N}` taken from the outer dimensions of the inputs:

```wl
A = TTensorCreate[{{1., 2., 3.}}];
B = TGlorot[{3, 4}];
TTensorShape @ TRealize @ TMatMul[A, B]
```
<!-- => {1, 4} -->
