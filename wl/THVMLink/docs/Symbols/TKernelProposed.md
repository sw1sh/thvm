---
Template: Symbol
Name: TKernelProposed
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TKernelProposed
Keywords: [kernel, opt, propose, autotune, UPCAST, UNROLL]
SeeAlso: [TKernelApplyOpt, TKernel, TKernelCount, TKernelSource, TOpt]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TKernelProposed]()[*kid*]</code> returns a list of <code>[TOpt]()[...]</code> candidate optimizations suggested by the C-side shape heuristic proposer for kernel *kid*.

## Details & Options

- On the CPU backend the heuristics propose `UNROLL` on reduce axes and `UPCAST` on elementwise output axes where the extent divides evenly.
- With `DEV=metal`, recognized f32 GEMM kernels propose tensor-core tile sizes; with `THVM_TILE=1`, supported tile kernels propose `LOCAL` and `GROUP` factors.
- Feed a proposed <code>[TOpt]()</code> to <code>[TKernelApplyOpt]()</code>; the autotuner consumes the whole list.
- Obtain a `kid` from <code>[TKernelCount]()[] - 1</code> after a realize (see <code>[TKernelCount]()</code>).

## Basic Examples

A sum-of-squares kernel over an 8-element input proposes `UNROLL` factors on its reduce axis:

```wl
x = TTensorCreate[Range[1., 8.]];
TRealize[Total[x^2]];
kid = TKernelCount[] - 1;
TKernelProposed[kid]
```
<!-- => {TOpt[UNROLL, 0, 8], TOpt[UNROLL, 0, 4], TOpt[UNROLL, 0, 2]} -->

## Properties and Relations

The result is a list of <code>[TOpt]()</code> candidates ready for <code>[TKernelApplyOpt]()</code>:

```wl
x = TTensorCreate[Range[1., 8.]];
TRealize[Total[x^2]];
kid = TKernelCount[] - 1;
MatchQ[TKernelProposed[kid], {__TOpt}]
```
<!-- => True -->
