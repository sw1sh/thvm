---
Template: Symbol
Name: TKernelProposed
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TKernelProposed
Keywords: [kernel, optimization, proposal, candidates, autotune, upcast, unroll]
SeeAlso: [TKernelApplyOpt, TKernel, TKernelSource, TKernelFlops, TMaterialize]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TKernelProposed]()[*kid*]</code> gives a list of candidate optimizations for the loop schedule of the kernel with id *kid*.

<!-- #| annotation: 26.10.19: The candidates come from the C-side shape heuristic proposer, the counterpart of the action space tinygrad's search walks. On the CPU it proposes UNROLL of the reduction axis and UPCAST of the first output axis by powers of two up to 16 that divide the axis length. With the Metal backend, recognized f32 GEMM kernels also propose TC tile sizes, and with THVM_TILE=1 supported tile kernels propose LOCAL and GROUP factors; those cases are not exercised on this page. TKernelAutotune and TKernelVariants consume this list. TOpt has no reference page yet. -->

## Details & Options

- Each candidate is `TOpt["`*op*`", `*axis*`, `*arg*`]`, the form <code>[TKernelApplyOpt]()</code> takes, and displays as a summary box.
- On the central processing unit (CPU), a kernel with a reduction gets `"UNROLL"` candidates on its reduction axis, and an elementwise kernel gets `"UPCAST"` candidates on its first output axis.
- The factors are the powers of two up to 16 that divide the length of the axis, largest first. An axis whose length has no such factor gets no candidates.
- The candidates are computed from the current plan, so after an optimization is applied they cover only what is left of the split axis.
- The list is empty when no candidate fits, and for an id with no kernel.

## Basic Examples

A tensor:

```wl
x = TTensorCreate[Range[1., 8.]]
```

<!-- => a TTensor summary box: shape 8, dtype f32 -->

The id of the kernel for the sum of its squares:

```wl
kid = TKernel[TMaterialize[Total[x^2]]]["Kid"]
```

<!-- => the kernel id, a positive integer -->

The candidates unroll the reduction axis by 8, 4 or 2:

```wl
TKernelProposed[kid]
```

<!-- => {TOpt["UNROLL", 0, 8], TOpt["UNROLL", 0, 4], TOpt["UNROLL", 0, 2]}, each as a TOpt summary box -->

## Scope

A tensor of length 32:

```wl
x = TTensorCreate[Range[1., 32.]]
```

<!-- => a TTensor summary box: shape 32, dtype f32 -->

An elementwise kernel gets `"UPCAST"` candidates on its output axis, by powers of two up to 16:

```wl
TKernelProposed[TKernel[TMaterialize[x^2]]["Kid"]]
```

<!-- => {TOpt["UPCAST", 0, 16], TOpt["UPCAST", 0, 8], TOpt["UPCAST", 0, 4], TOpt["UPCAST", 0, 2]} -->

---

A 4 x 4 matrix:

```wl
m = TTensorCreate[ArrayReshape[Range[1., 16.], {4, 4}]]
```

<!-- => a TTensor summary box: shape 4 x 4, dtype f32 -->

The kernel of its row sums reduces axis 1, so the candidates unroll axis 1:

```wl
TKernelProposed[TKernel[TMaterialize[ArrayReduce[Total, m, 2]]]["Kid"]]
```

<!-- => {TOpt["UNROLL", 1, 4], TOpt["UNROLL", 1, 2]} -->

The kernel of its square reduces axis 2:

```wl
TKernelProposed[TKernel[TMaterialize[m . m]]["Kid"]]
```

<!-- => {TOpt["UNROLL", 2, 4], TOpt["UNROLL", 2, 2]} -->

---

An axis of length 6 has only the factor 2 among the powers of two:

```wl
TKernelProposed[TKernel[TMaterialize[TTensorCreate[Range[1., 6.]]^2]]["Kid"]]
```

<!-- => {TOpt["UPCAST", 0, 2]} -->

## Applications

A tensor:

```wl
x = TTensorCreate[Range[1., 8.]]
```

<!-- => a TTensor summary box: shape 8, dtype f32 -->

The id of the kernel for the sum of its squares:

```wl
kid = TKernel[TMaterialize[Total[x^2]]]["Kid"]
```

<!-- => the kernel id, a positive integer -->

Apply the first candidate:

```wl
TKernelApplyOpt[kid, First[TKernelProposed[kid]]]
```

<!-- => a TKernelOpts summary box: applied 1, axes REDUCE 1, UNROLL 8 -->

The optimized kernel computes the same sum:

```wl
Normal[TKernel[kid][]]
```

<!-- => {204.} -->

## Properties and Relations

A tensor of length 32:

```wl
x = TTensorCreate[Range[1., 32.]]
```

<!-- => a TTensor summary box: shape 32, dtype f32 -->

The id of the kernel for the sum of its squares:

```wl
kid = TKernel[TMaterialize[Total[x^2]]]["Kid"]
```

<!-- => the kernel id, a positive integer -->

Unroll its reduction by 8:

```wl
TKernelApplyOpt[kid, TOpt["UNROLL", 0, 8]]
```

<!-- => a TKernelOpts summary box: applied 1, axes REDUCE 4, UNROLL 8 -->

The candidates now split what is left of the reduction, an axis of length 4:

```wl
TKernelProposed[kid]
```

<!-- => {TOpt["UNROLL", 0, 4], TOpt["UNROLL", 0, 2]} -->

Unroll that by 4 as well:

```wl
TKernelApplyOpt[kid, TOpt["UNROLL", 0, 4]]
```

<!-- => a TKernelOpts summary box: applied 2 -->

Nothing is left to split:

```wl
TKernelProposed[kid]
```

<!-- => {} -->

## Possible Issues

An axis of length 7 has no power-of-two factor, so the kernel gets no candidates:

```wl
TKernelProposed[TKernel[TMaterialize[TTensorCreate[Range[1., 7.]]^2]]["Kid"]]
```

<!-- => {} -->

---

An id past the newest kernel also gives an empty list:

```wl
TKernelProposed[TKernelCount[] + 100]
```

<!-- => {} -->
