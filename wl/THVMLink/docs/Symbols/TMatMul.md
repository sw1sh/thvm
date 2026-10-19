---
Template: Symbol
Name: TMatMul
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TMatMul
Keywords: [matrix product, matmul, GEMM, BLAS, linear algebra, Dot]
SeeAlso: [Dot, TRealize, TGrad, TGlorot, TKernel]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TMatMul]()[*a*, *b*]</code> gives the matrix product of the tensors *a* and *b*.

<!-- #| annotation: 26.10.19: TMatMul lowers tinygrad-style: reshape and expand both operands to a common {m, k, n} shape, an elementwise MUL, then a SUM reduce over axis 1. The CPU dispatcher recognizes that pattern and routes it to cblas_sgemm (DispatchKind "blas-gemm"); a product whose two operands are the same tensor is not matched and compiles as an ordinary loop kernel ("jit"). The Dot UpValue on TTerm dispatches rank-2 by rank-2 here and handles the vector cases with TMatVec and TDot. -->

## Details & Options

- *a* has shape `{`*m*`, `*k*`}` and *b* has shape `{`*k*`, `*n*`}`; the product has shape `{`*m*`, `*n*`}`.
- <code>[TMatMul]()[*a*, *b*]</code> gives a lazy [TTerm](). <code>[TRealize]()</code> computes it, and <code>[Normal]()</code> reads the values back.
- For two matrices, <code>[Dot]()[*a*, *b*]</code>, written *a* . *b*, builds the same product. [Dot]() also multiplies a matrix and a vector, or two vectors.
- Under <code>[TGrad]()</code>, the gradient flows to both factors.
- On the central processing unit (CPU), the product of two different `"f32"` matrices is computed by the matrix multiplication of the Basic Linear Algebra Subprograms (BLAS), reported as the dispatch kind `"blas-gemm"` of its kernel.
- Two `"i32"` matrices multiply in integer arithmetic.
- Both arguments must be matrices whose inner dimensions agree; otherwise [TMatMul]() issues the message `TMatMul::dims` and returns unevaluated. An argument that is not a [TTerm]() returns unevaluated.

## Basic Examples

A 2 x 2 matrix:

```wl
a = TTensorCreate[{{1., 2.}, {3., 4.}}]
```

<!-- => a TTensor summary box: shape 2 x 2, dtype f32 -->

Another:

```wl
b = TTensorCreate[{{5., 6.}, {7., 8.}}]
```

<!-- => a TTensor summary box: shape 2 x 2, dtype f32 -->

Their product, as a lazy graph:

```wl
p = TMatMul[a, b]
```

<!-- => a TUOp summary box of kind REDUCE -->

Realize it and read the values:

```wl
Normal[TRealize[p]]
```

<!-- => {{19., 22.}, {43., 50.}} -->

## Scope

A row and a 3 x 4 matrix give a row of length 4:

```wl
Normal[TRealize[TMatMul[TTensorCreate[{{1., 2., 3.}}], TTensorCreate[ArrayReshape[Range[1., 12.], {3, 4}]]]]]
```

<!-- => {{38., 44., 50., 56.}} -->

---

A 2 x 2 matrix:

```wl
a = TTensorCreate[{{1., 2.}, {3., 4.}}]
```

<!-- => a TTensor summary box: shape 2 x 2, dtype f32 -->

A factor may itself be a lazy graph, here the transpose:

```wl
Normal[TRealize[TMatMul[Transpose[a], a]]]
```

<!-- => {{10., 14.}, {14., 20.}} -->

A column matrix gives the row sums:

```wl
Normal[TRealize[TMatMul[a, TTensorCreate[{{1.}, {1.}}]]]]
```

<!-- => {{3.}, {7.}} -->

---

Integer matrices multiply exactly:

```wl
Normal[TRealize[TMatMul[TTensorCreate[{{1, 2}, {3, 4}}, "i32"], TTensorCreate[{{0, 1}, {1, 0}}, "i32"]]]]
```

<!-- => {{2, 1}, {4, 3}} -->

## Applications

Three points in the plane, one per row:

```wl
pts = TTensorCreate[{{1., 0.}, {0., 1.}, {1., 1.}}]
```

<!-- => a TTensor summary box: shape 3 x 2, dtype f32 -->

Multiplying the rows by a rotation matrix turns every point by a quarter turn:

```wl
Normal[TRealize[TMatMul[pts, TTensorCreate[{{0., 1.}, {-1., 0.}}]]]]
```

<!-- => {{0., 1.}, {-1., 0.}, {-1., 1.}} -->

## Properties and Relations

A 2 x 2 matrix:

```wl
a = TTensorCreate[{{1., 2.}, {3., 4.}}]
```

<!-- => a TTensor summary box: shape 2 x 2, dtype f32 -->

Another:

```wl
b = TTensorCreate[{{5., 6.}, {7., 8.}}]
```

<!-- => a TTensor summary box: shape 2 x 2, dtype f32 -->

[Dot]() on the two tensors builds the same product:

```wl
Normal[TRealize[TMatMul[a, b]]] === Normal[TRealize[a . b]]
```

<!-- => True -->

It agrees with [Dot]() on the plain matrices:

```wl
Normal[TRealize[TMatMul[a, b]]] === Normal[a] . Normal[b]
```

<!-- => True -->

The kernel of the product:

```wl
k = TKernel[TMaterialize[TMatMul[a, b]]]
```

<!-- => a TKernel summary box: 2 inputs, output numel 4, output dtype f32 -->

Run it:

```wl
Normal[k[]]
```

<!-- => {{19., 22.}, {43., 50.}} -->

On the CPU it ran through BLAS:

```wl
k["DispatchKind"]
```

<!-- => "blas-gemm" -->

---

A 2 x 2 matrix:

```wl
a = TTensorCreate[{{1., 2.}, {3., 4.}}]
```

<!-- => a TTensor summary box: shape 2 x 2, dtype f32 -->

Another:

```wl
b = TTensorCreate[{{5., 6.}, {7., 8.}}]
```

<!-- => a TTensor summary box: shape 2 x 2, dtype f32 -->

Differentiate the sum of the entries of the product:

```wl
TGrad[Total[TMatMul[a, b], All]]
```

<!-- => a TUOp summary box of kind REDUCE -->

The gradient with respect to *a* repeats the row sums of *b*:

```wl
Normal[TRealize[TGradOf[a]]]
```

<!-- => {{11., 15.}, {11., 15.}} -->

The gradient with respect to *b* repeats the column sums of *a*:

```wl
Normal[TRealize[TGradOf[b]]]
```

<!-- => {{4., 4.}, {6., 6.}} -->

## Possible Issues

A 2 x 2 matrix:

```wl
a = TTensorCreate[{{1., 2.}, {3., 4.}}]
```

<!-- => a TTensor summary box: shape 2 x 2, dtype f32 -->

The inner dimensions of a 2 x 2 and a 1 x 3 matrix do not agree, so [TMatMul]() issues a message and returns unevaluated:

```wl
TMatMul[a, TTensorCreate[{{1., 2., 3.}}]]
```

<!-- => the message TMatMul::dims is issued and the expression returns unevaluated -->

---

Plain lists are not tensors, and [TMatMul]() returns unevaluated:

```wl
TMatMul[{{1., 2.}, {3., 4.}}, {{5., 6.}, {7., 8.}}]
```

<!-- => TMatMul[{{1., 2.}, {3., 4.}}, {{5., 6.}, {7., 8.}}] -->
