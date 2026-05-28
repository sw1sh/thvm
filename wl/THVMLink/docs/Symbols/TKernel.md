---
Template: Symbol
Name: TKernel
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TKernel
Keywords: [kernel, JIT, codegen, autotune, Metal, CPU, dispatch]
SeeAlso: [TKernelSource, TKernelOpts, TOpt, TKernelApplyOpt, TKernelAutotune, TKernelProposed, TKernelProfile, TMemoryPlan]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TKernel]()[$t_TTerm$]</code> wraps a UOP_KERNEL term as a typed object with a queryable property surface.

<code>[TKernel]()[$kid_Integer$]</code> resolves a kernel id back to its pinned heap term and wraps that.

A `TKernel` auto-coerces to its underlying `TTerm` inside any UOp constructor.

## Details & Options

- Use `Information[k, "Properties"]` for the full property list and `k["Name"]` to query one. Calling `k[]` dispatches the kernel (same as <code>[TKernelDispatch]()</code>).
- Key properties:
  - `"Source"` - the C99 or Metal source text. <code>[TKernelSource]()[kid, backend]</code> picks the back end explicitly.
  - `"Flops"` - static FLOPS estimate over the lifted UOp DAG.
  - `"DispatchKind"` - the route the last fire took ("jit", "blas-gemm", "metal-jit", "metal-tile", "interpreter", ...).
  - `"DispatchCount"`, `"TotalUs"` - cumulative fire counters since `TInit`.
  - `"JitDylibPath"` - the on-disk path of the JIT-cached `.dylib` (deterministic from the program hash).
- The kernel's scheduling plan is mutable via <code>[TKernelApplyOpt]()</code> with <code>[TOpt]()</code> actions (UPCAST, UNROLL, LOCAL, GLOBAL, TC, ...).
- <code>[TKernelAutotune]()</code> benchmarks every <code>[TKernelProposed]()</code> candidate and applies the winner, keyed by the kernel's structural <code>[TKernelProgramKey]()</code> so other kernels with the same shape inherit it.

## Basic Examples

Fire a small kernel and inspect what dispatch route it took:

```wl
Needs["THVMLink`"];
TInit[];
x = TTensorCreate[Range[1., 16.]];
y = TTensorCreate[Range[16., 1., -1.]];
TRealize @ TUOpAdd[x, y];
k = TKernel[1];
{k["Name"], k["DispatchKind"], k["DispatchCount"]}
```
<!-- => {"...", "jit" or "blas-...", 1} -->

## Scope

Read the lifted source the JIT compiled:

```wl
src = TKernelSource[1, "C"];
StringLength[src]
```
<!-- => length of the rendered C99 string; or 0 when kernel_lift_to_uop declined -->

---

Inspect the axis-typed scheduling plan and the proposer's candidates:

```wl
{TKernelOpts[1], TKernelProposed[1]}
```
<!-- => {TKernelOpts[<|"AxisTypes" -> {...}, "Applied" -> {}|>], {TOpt["UPCAST", axis, factor], ...}} -->

## Applications

Run the autotuner across every unique kernel shape, then re-fire to pick up the winner:

```wl
TKernelAutotuneUnique[];
TRealize @ TUOpAdd[x, y];
TKernel[1][ "DispatchCount"]
```
<!-- => 2 -- the second fire used the post-autotune schedule -->

---

Group running kernels by their lifted Metal source to find structural duplicates:

```wl
TKernelDuplicateGroups[]
```
<!-- => <|"hash..." -> {kid1, kid2, ...}, ...|> -->

## Properties and Relations

A `TKernel` auto-coerces into UOp graphs, so it can be passed wherever a `TTerm` is expected:

```wl
k = TKernel[1];
TUOpKind[k]
```
<!-- => "KERNEL" -->

## Possible Issues

A kernel whose lifted DAG carries unresolved BUFFERIZE or TEN leaves emits `buf<NNNN>` undeclared identifiers in MSL and fails Metal compile. The audit helpers flag this before dispatch:

```wl
TKernelAuditLeaks[]
```
<!-- => {<|Kid -> _, BufferizeLeak -> True/False, TenLeak -> True/False|>, ...} -->

## Neat Examples

Per-kernel introspection plus the memory planner pinpoints the hot allocation:

```wl
top = TKernelMemoryTopProducers[5];
top // First
```
<!-- => <|Kid -> _, OutputBytes -> _, MSLLines -> _, HashGroup -> {...}|> -->
