---
Template: Symbol
Name: TKernelSource
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TKernelSource
Keywords: [kernel, source, codegen, C, Metal, render]
SeeAlso: [TKernel, TKernelApplyOpt, TKernelFlops, TMaterialize, TKernelCount]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TKernelSource]()[*kid*]</code> gives the source code of the kernel with id *kid* for the active backend.

<code>[TKernelSource]()[*kid*, "*backend*"]</code> gives the source code for the named backend, `"C"` or `"Metal"`.

<!-- #| annotation: 26.10.19: The source is rendered from the kernel's lifted UOp DAG under its current KpSchedule plan, by the C renderer (which the CPU backend compiles with clang -O3 into a dylib on first dispatch) or the Metal Shading Language renderer. The default backend follows the DEV environment variable, C otherwise. An empty string means kernel_lift_to_uop declined (too many inputs, no scalar arena) and the kernel runs in the per-op interpreter. There is no CUDA renderer behind this function. TKernel[kid]["Source", backend], which the usage message documents, returns unevaluated (reported 26.10.19). -->

## Details & Options

- With `"C"`, the source is the C function the backend for the central processing unit (CPU) compiles to native code on the kernel's first dispatch. With `"Metal"`, it is a Metal Shading Language compute kernel.
- The default is the active backend, which on the CPU is `"C"`.
- Rendering does not run the kernel, so the source of any scheduled kernel can be read, for any backend.
- The source follows the kernel's current loop plan, so it changes when <code>[TKernelApplyOpt]()</code> splits or reorders an axis.
- <code>[TKernelSource]()[*kid*]</code> equals <code>[TKernel]()[*kid*]["Source"]</code>.
- It gives an empty string for an id with no kernel, and a [Failure]() for a backend other than `"C"` or `"Metal"`.

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

Its C source, a reduction loop accumulating the squares into the one output element:

```wl
TKernelSource[kid]
```

<!-- => "#include <stdint.h> ... void k(void *out_v, const void *const *ins_v, unsigned n, const unsigned *in_numels, const unsigned *kvar_vals) { ... float _acc0 = 0.0f; for (uint a0 = 0; a0 < 8; a0++) /*reduce*/ { _acc0 = _acc0 + (in0[a0] * in0[a0]); } out[0] = _acc0; }" -->

## Scope

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

The same kernel as a Metal compute kernel:

```wl
TKernelSource[kid, "Metal"]
```

<!-- => "#include <metal_stdlib> using namespace metal; kernel void k(device float *out [[ buffer(0) ]], device const float *in0 [[ buffer(1) ]], ...) { float _acc0 = 0.0f; #pragma unroll(8) for (uint a0 = 0; a0 < 8; a0++) /*reduce*/ { _acc0 = _acc0 + (in0[a0] * in0[a0]); } out[0] = _acc0; }" -->

---

A tensor:

```wl
x = TTensorCreate[Range[1., 8.]]
```

<!-- => a TTensor summary box: shape 8, dtype f32 -->

An elementwise kernel is one loop over the output; [Exp]() appears as a power of 2:

```wl
TKernelSource[TKernel[TMaterialize[Exp[x]]]["Kid"]]
```

<!-- => "... for (uint a0 = 0; a0 < 8; a0++) { out[a0] = exp2((in0[a0] * THVM_BITCAST(float, 0x3fb8aa3bu))); } ..." -->

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

The source before any optimization has no unrolled loop:

```wl
StringContainsQ[TKernelSource[kid], "unroll_count"]
```

<!-- => False -->

Unroll the reduction by 4:

```wl
TKernelApplyOpt[kid, TOpt["UNROLL", 0, 4]]
```

<!-- => a TKernelOpts summary box: applied 1 -->

The source now carries the unroll:

```wl
StringContainsQ[TKernelSource[kid], "unroll_count(4)"]
```

<!-- => True -->

## Properties and Relations

A tensor:

```wl
x = TTensorCreate[Range[1., 8.]]
```

<!-- => a TTensor summary box: shape 8, dtype f32 -->

The kernel of its square:

```wl
k = TKernel[TMaterialize[x^2]]
```

<!-- => a TKernel summary box: 1 input, output numel 8, output dtype f32 -->

The source is the `"Source"` property of the kernel object:

```wl
TKernelSource[k["Kid"]] === k["Source"]
```

<!-- => True -->

## Possible Issues

An id past the newest kernel gives an empty string:

```wl
TKernelSource[TKernelCount[] + 100]
```

<!-- => "" -->

---

A tensor:

```wl
x = TTensorCreate[Range[1., 8.]]
```

<!-- => a TTensor summary box: shape 8, dtype f32 -->

The id of the kernel for its square:

```wl
kid = TKernel[TMaterialize[x^2]]["Kid"]
```

<!-- => the kernel id, a positive integer -->

Only `"C"` and `"Metal"` are backends, spelled with that capitalization:

```wl
TKernelSource[kid, "CUDA"]
```

<!-- => Failure["UnknownBackend", <|"Message" -> "TKernelSource backend must be \"C\" or \"Metal\"", "Backend" -> "CUDA"|>] -->
