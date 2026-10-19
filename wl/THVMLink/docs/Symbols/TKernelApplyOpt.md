---
Template: Symbol
Name: TKernelApplyOpt
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TKernelApplyOpt
Keywords: [kernel, optimization, schedule, upcast, unroll, loop, autotune]
SeeAlso: [TKernelProposed, TKernel, TKernelSource, TKernelFlops, TMaterialize]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TKernelApplyOpt]()[*kid*, *opt*]</code> applies the optimization *opt* to the loop schedule of the kernel with id *kid* and gives the kernel's updated plan.

<!-- #| annotation: 26.10.19: TOpt and the plan mirror tinygrad's Opt(OptOps.X, axis, arg) and Kernel.apply_opt; the axis structure and the rewrite live on the C side (the KpSchedule slot of the kernel), and WL only wraps the result as TKernelOpts. Kernels with the same nonzero TKernelProgramKey share one schedule slot, so an opt applied to one representative reaches every kernel of that program shape. On the first dispatch the CPU backend runs its own hand-opt heuristic over the plan, which is why an extra UNROLL can appear after a dispatch. TOpt and TKernelOpts have no reference pages yet. Applying GLOBAL or LOCAL to a CPU kernel makes the C codegen fail and the kernel falls back to the interpreter with a misleading compiler message (reported 26.10.19). -->

## Details & Options

- *opt* is `TOpt["`*op*`", `*axis*`, `*arg*`]`, the form <code>[TKernelProposed]()</code> gives. *axis* counts from 0 over the axes of the current plan.
- The operations *op* are:

  | | |
  | --- | --- |
  | `"UPCAST"` | split an output axis by the factor *arg* and compute the inner part as one block |
  | `"UNROLL"` | split a reduction axis by the factor *arg* and unroll the inner loop |
  | `"SWAP"` | exchange the loop order of *axis* and the axis *arg* |
  | `"LOCAL"`, `"GROUP"`, `"GROUPTOP"` | split an axis by *arg* into threads of a group on a graphics processing unit (GPU) |
  | `"GLOBAL"` | map the output axis of length *arg* onto the GPU grid |
  | `"TC"` | use tensor-core tiles of size *arg* for a recognized f32 matrix product |

- The result is a `TKernelOpts` object holding the type of each axis, the shape of the loops after splitting, and the optimizations applied so far, in order. It displays as a summary box.
- Optimizations accumulate: each one applies to the plan the previous ones left.
- The plan changes the code the kernel compiles to, not the values it computes. It takes effect when the kernel is compiled, at its first dispatch.
- At its first dispatch the backend for the central processing unit (CPU) adds its own default optimizations to the plan.
- An *opt* that does not fit the plan, such as an axis out of range or a factor that does not divide the axis length, gives `Failure["opt-rejected", …]` and leaves the plan unchanged. `"PADTO"` and `"NOLOCALS"` are reserved and always rejected.

## Basic Examples

A tensor:

```wl
x = TTensorCreate[Range[1., 8.]]
```

<!-- => a TTensor summary box: shape 8, dtype f32 -->

The id of the kernel for the sum of its squares, scheduled but not run:

```wl
kid = TKernel[TMaterialize[Total[x^2]]]["Kid"]
```

<!-- => the kernel id, a positive integer -->

Unrolling the reduction axis by 4 leaves a reduction axis of length 2 and an unrolled axis of length 4:

```wl
TKernelApplyOpt[kid, TOpt["UNROLL", 0, 4]]
```

<!-- => a TKernelOpts summary box: applied 1, axes REDUCE 2, UNROLL 4 -->

The kernel's C source has the split loop, with the inner loop marked for unrolling:

```wl
TKernelSource[kid]
```

<!-- => the C source of void k(...), whose reduction is a loop over a0 < 2 enclosing #pragma clang loop unroll_count(4) and a loop over a1 < 4 -->

The kernel still computes the same sum:

```wl
Normal[TKernel[kid][]]
```

<!-- => {204.} -->

## Scope

A tensor:

```wl
x = TTensorCreate[Range[1., 8.]]
```

<!-- => a TTensor summary box: shape 8, dtype f32 -->

The id of the kernel for its elementwise square:

```wl
kid = TKernel[TMaterialize[x^2]]["Kid"]
```

<!-- => the kernel id, a positive integer -->

An elementwise kernel has an output axis, which `"UPCAST"` splits:

```wl
TKernelApplyOpt[kid, TOpt["UPCAST", 0, 4]]
```

<!-- => a TKernelOpts summary box: applied 1, axes LOOP 2, UPCAST 4 -->

The squares are unchanged:

```wl
Normal[TKernel[kid][]]
```

<!-- => {1., 4., 9., 16., 25., 36., 49., 64.} -->

---

A 4 x 4 matrix:

```wl
m = TTensorCreate[ArrayReshape[Range[1., 16.], {4, 4}]]
```

<!-- => a TTensor summary box: shape 4 x 4, dtype f32 -->

The id of the kernel for its square, with two output axes and one reduction axis:

```wl
kid = TKernel[TMaterialize[m . m]]["Kid"]
```

<!-- => the kernel id, a positive integer -->

Unroll the reduction, axis 2:

```wl
TKernelApplyOpt[kid, TOpt["UNROLL", 2, 4]]
```

<!-- => a TKernelOpts summary box: applied 1, axes LOOP 4, LOOP 4, REDUCE 1, UNROLL 4 -->

Exchange the order of the two output loops:

```wl
TKernelApplyOpt[kid, TOpt["SWAP", 0, 1]]
```

<!-- => a TKernelOpts summary box: applied 2 -->

Split the second output axis by 2:

```wl
TKernelApplyOpt[kid, TOpt["UPCAST", 1, 2]]
```

<!-- => a TKernelOpts summary box: applied 3, axes LOOP 4, LOOP 2, UPCAST 2, REDUCE 1, UNROLL 4 -->

The product is the same as without the optimizations:

```wl
Normal[TKernel[kid][]] === Normal[m] . Normal[m]
```

<!-- => True -->

## Applications

A tensor:

```wl
x = TTensorCreate[Range[1., 8.]]
```

<!-- => a TTensor summary box: shape 8, dtype f32 -->

The id of the kernel for its sum of squares:

```wl
kid = TKernel[TMaterialize[Total[x^2]]]["Kid"]
```

<!-- => the kernel id, a positive integer -->

The candidate optimizations for this kernel:

```wl
TKernelProposed[kid]
```

<!-- => {TOpt["UNROLL", 0, 8], TOpt["UNROLL", 0, 4], TOpt["UNROLL", 0, 2]} -->

Apply the first one, unrolling the whole reduction:

```wl
TKernelApplyOpt[kid, First[TKernelProposed[kid]]]
```

<!-- => a TKernelOpts summary box: applied 1, axes REDUCE 1, UNROLL 8 -->

## Properties and Relations

A tensor:

```wl
x = TTensorCreate[Range[1., 8.]]
```

<!-- => a TTensor summary box: shape 8, dtype f32 -->

The id of the kernel for its sum of squares:

```wl
kid = TKernel[TMaterialize[Total[x^2]]]["Kid"]
```

<!-- => the kernel id, a positive integer -->

Unroll by 4:

```wl
TKernelApplyOpt[kid, TOpt["UNROLL", 0, 4]]
```

<!-- => a TKernelOpts summary box: applied 1 -->

The proposals now start from the split plan, so only a factor of the remaining reduction length 2 is left:

```wl
TKernelProposed[kid]
```

<!-- => {TOpt["UNROLL", 0, 2]} -->

## Possible Issues

A tensor:

```wl
x = TTensorCreate[Range[1., 8.]]
```

<!-- => a TTensor summary box: shape 8, dtype f32 -->

The id of the kernel for its sum of squares:

```wl
kid = TKernel[TMaterialize[Total[x^2]]]["Kid"]
```

<!-- => the kernel id, a positive integer -->

The plan has only axis 0, so an optimization of axis 99 is rejected:

```wl
TKernelApplyOpt[kid, TOpt["UNROLL", 99, 4]]
```

<!-- => Failure["opt-rejected", <|"Kid" -> kid, "Opt" -> TOpt["UNROLL", 99, 4]|>] -->

A factor of 3 does not divide the axis length 8:

```wl
TKernelApplyOpt[kid, TOpt["UNROLL", 0, 3]]
```

<!-- => Failure["opt-rejected", <|"Kid" -> kid, "Opt" -> TOpt["UNROLL", 0, 3]|>] -->

`"PADTO"` is reserved and always rejected:

```wl
TKernelApplyOpt[kid, TOpt["PADTO", 0, 4]]
```

<!-- => Failure["opt-rejected", <|"Kid" -> kid, "Opt" -> TOpt["PADTO", 0, 4]|>] -->

---

A tensor:

```wl
x = TTensorCreate[Range[1., 8.]]
```

<!-- => a TTensor summary box: shape 8, dtype f32 -->

The id of the kernel for its sum of squares:

```wl
kid = TKernel[TMaterialize[Total[x^2]]]["Kid"]
```

<!-- => the kernel id, a positive integer -->

Unroll by 4:

```wl
TKernelApplyOpt[kid, TOpt["UNROLL", 0, 4]]
```

<!-- => a TKernelOpts summary box: applied 1 -->

The first dispatch compiles the kernel:

```wl
Normal[TKernel[kid][]]
```

<!-- => {204.} -->

The CPU backend has added its own unroll of the remaining reduction to the plan:

```wl
TKernelOpts[kid]
```

<!-- => a TKernelOpts summary box: applied 2, with TOpt["UNROLL", 0, 4] followed by TOpt["UNROLL", 0, 2] -->
