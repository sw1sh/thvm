---
Template: Symbol
Name: TKernel
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TKernel
Keywords: [kernel, JIT, codegen, autotune, Metal, CPU, dispatch]
SeeAlso: [TKernelSource, TKernelOpts, TOpt, TKernelApplyOpt, TKernelAutotune, TKernelProposed, TKernelProfile, TMemoryPlan]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TKernel]()[*t*]</code> wraps a `UOP_KERNEL` `TTerm` *t* as a typed object with a queryable property surface.

<code>[TKernel]()[*kid*]</code> resolves a kernel id *kid* back to its pinned heap term and wraps that.

A `TKernel` auto-coerces to its underlying `TTerm` inside any UOp constructor.

## Details & Options

- Use <code>[Information]()[*k*, "Properties"]</code> for the full property list and `k["DispatchKind"]` to query one. Calling `k[]` dispatches the kernel (same as <code>[TKernelDispatch]()</code>).
- Key properties:
  - `"Source"` - the C99 or Metal source text. <code>[TKernelSource]()[*kid*, *backend*]</code> picks the back end explicitly.
  - `"Flops"` - static FLOPS estimate over the lifted UOp DAG.
  - `"DispatchKind"` - the route the last fire took ("jit", "blas-gemm", "metal-jit", "metal-tile", "interpreter", ...).
  - `"DispatchCount"`, `"TotalUs"` - cumulative fire counters since <code>[TInit]()</code>.
  - `"JitDylibPath"` - the on-disk path of the JIT-cached `.dylib` (deterministic from the program hash).
- The kernel's scheduling plan is mutable via <code>[TKernelApplyOpt]()</code> with <code>[TOpt]()</code> actions (UPCAST, UNROLL, LOCAL, GLOBAL, TC, ...).
- <code>[TKernelAutotune]()</code> benchmarks every <code>[TKernelProposed]()</code> candidate and applies the winner, keyed by the kernel's structural <code>[TKernelProgramKey]()</code> so other kernels with the same shape inherit it.

## Basic Examples

Fire a small kernel. Its id is the last entry in the kernel table, so derive it from <code>[TKernelCount]()</code> rather than hardcoding an absolute id (absolute kids drift with accumulated runtime state):

```wl
x = TTensorCreate[Range[1., 16.]];
y = TTensorCreate[Range[16., 1., -1.]];
TRealize @ (x + y);
kid = TKernelCount[] - 1;
k = TKernel[kid]
```
<!-- => TKernel[<|"Term" -> TTerm[...], "Kid" -> _|>] -- the summary box: kid, output shape/dtype, inputs, fired -->

The box shows the route the last fire took:

```wl
k["DispatchKind"]
```
<!-- => "jit" (or "blas-...", "metal-jit", ...) -->

and the cumulative fire count:

```wl
k["DispatchCount"]
```
<!-- => 1 -->

## Scope

Read the lifted source the JIT compiled:

```wl
x = TTensorCreate[Range[1., 16.]];
y = TTensorCreate[Range[16., 1., -1.]];
TRealize @ (x + y);
kid = TKernelCount[] - 1;
src = TKernelSource[kid, "C"];
StringLength[src]
```
<!-- => 788 -- length of the rendered C99 string; or 0 when kernel_lift_to_uop declined -->

---

Inspect the axis-typed scheduling plan:

```wl
TKernelOpts[kid]
```
<!-- => TKernelOpts[<|"Kid" -> _, "AxisTypes" -> {"LOOP", "UPCAST"}, "FullShape" -> {4, 4}, "Applied" -> {TOpt["UPCAST", 0, 4]}|>] -->

---

and the proposer's candidate actions:

```wl
TKernelProposed[kid]
```
<!-- => {TOpt["UPCAST", 0, 4], TOpt["UPCAST", 0, 2]} -->

## Applications

Run the autotuner across every unique kernel shape, then re-fire to pick up the winner:

```wl
x = TTensorCreate[Range[1., 16.]];
y = TTensorCreate[Range[16., 1., -1.]];
TRealize @ (x + y);
kid = TKernelCount[] - 1;
TKernelAutotuneUnique[];
TRealize @ (x + y);
TKernel[kid]["DispatchCount"]
```
<!-- => ~19 the first time this shape is tuned (autotune fires every candidate to benchmark it, plus the final re-fire); the count is lower if the program key was already tuned and cached -->

---

Group running kernels by their lifted Metal source to find structural duplicates:

```wl
TKernelDuplicateGroups[]
```
<!-- => <|"hash..." -> {kid1, kid2, ...}, ...|>; <||> when every live kernel is structurally unique -->

## Properties and Relations

A `TKernel` auto-coerces into UOp graphs, so it can be passed wherever a `TTerm` is expected:

```wl
x = TTensorCreate[Range[1., 16.]];
y = TTensorCreate[Range[16., 1., -1.]];
TRealize @ (x + y);
k = TKernel[TKernelCount[] - 1];
TUOpKind[k]
```
<!-- => "KERNEL" -->

## Possible Issues

A kernel whose lifted DAG carries unresolved BUFFERIZE or TEN leaves emits `buf<NNNN>` undeclared identifiers in MSL and fails Metal compile. The audit helpers flag this before dispatch:

```wl
TKernelAuditLeaks[]
```
<!-- => {<|Kid -> _, BufferizeLeak -> True/False, TenLeak -> True/False|>, ...}; {} when no live kernel leaks -->

## Neat Examples

Per-kernel introspection plus the memory planner pinpoints the hot allocation:

```wl
top = TKernelMemoryTopProducers[5];
top // First
```
<!-- => <|"Kid" -> _, "OutputBytes" -> _, "MSLLines" -> 19, "HashGroup" -> {...}|> -->
