---
Template: Symbol
Name: TKernelApplyOpt
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TKernelApplyOpt
Keywords: [kernel, opt, apply, schedule, UPCAST, UNROLL]
SeeAlso: [TKernelProposed, TKernel, TKernelOpts, TOpt, TKernelSource]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TKernelApplyOpt]()[*kid*, *opt*]</code> mutates kernel *kid*'s C-side scheduling plan by applying *opt*, a <code>[TOpt]()[*op*, *axis*, *arg*]</code>, and returns the updated [TKernelOpts]() wrapper.

## Details & Options

- Splits the indicated axis (`UPCAST` / `UNROLL` / `LOCAL` / `GROUP`), marks a full `LOOP` axis as `GLOBAL`, swaps two axes (`SWAP`), or records tensor-core metadata for recognized f32 GEMM kernels.
- Feed candidates from <code>[TKernelProposed]()</code>; obtain a `kid` from <code>[TKernelCount]()[] - 1</code> after a realize.
- Returns <code>[Failure]()["opt-rejected", ...]</code> on validation failure: axis out of range, arg not dividing the axis size, opts table full, unsupported tensor-core size, or a reserved opt not implemented.

## Basic Examples

A sum-of-squares kernel proposes `UNROLL` factors on its reduce axis:

```wl
x = TTensorCreate[Range[1., 8.]];
TRealize[Total[x^2]];
kid = TKernelCount[] - 1;
TKernelProposed[kid]
```
<!-- => {TOpt[UNROLL, 0, 8], TOpt[UNROLL, 0, 4], TOpt[UNROLL, 0, 2]} -->

Apply one of those proposed opts and read the resulting axis-type plan:

```wl
x = TTensorCreate[Range[1., 8.]];
TRealize[Total[x^2]];
kid = TKernelCount[] - 1;
res = TKernelApplyOpt[kid, TOpt["UNROLL", 0, 4]];
res[[1]]["AxisTypes"]
```
<!-- => {REDUCE, UNROLL} -->

## Possible Issues

An out-of-range axis is rejected with a <code>[Failure]()</code>:

```wl
x = TTensorCreate[Range[1., 8.]];
TRealize[Total[x^2]];
kid = TKernelCount[] - 1;
Head @ TKernelApplyOpt[kid, TOpt["UNROLL", 99, 4]]
```
<!-- => Failure -->
