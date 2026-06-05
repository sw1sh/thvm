---
Template: TechNote
Name: Overview
Title: A Tour of THVMLink
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/tutorial/Overview
Keywords: [interaction net, tensor, autodiff, kernel, visualization, ATP, tour]
RelatedGuides: [THVMLink]
RelatedTutorials: []
---

## What THVMLink is, in one paragraph

THVMLink is the Wolfram-language driver for `thvm`, a tensor-aware interaction-
combinator runtime. The C runtime holds one symbolic graph - lambda calculus,
the tensor UOp DAG, and the kernel schedule live in the same heap. The Wolfram
side wraps every cell as a `TTerm`, the tensor descriptors as `TTensor`
handles, the compiled kernels as `TKernel` typed objects, and renders the
whole runtime as an IC string diagram through `THeapGraph`. The same paclet
fires autodiff over the heap, plans buffer lifetimes, autotunes the CPU/CUDA/
Metal back ends, and proves equational theorems via the C-side ATP. This
tutorial walks the surface end to end with a single running example.

## Setting up

Initialize the runtime once per kernel:

```wl
Needs["THVMLink`"];
TInit[]
```
<!-- => True -->

`TInit` is idempotent within a session, and <code>[TReset]()</code> zeroes the heap without tearing the dylib down. The default <code>[TContext]()</code> selects the CPU back end; <code>[TContextNew]()["metal"]</code> opens a Metal context and <code>[TInContext]()</code> rebinds the active one for a body of code.

## Pure interaction nets

The combinator surface predates the tensor layer and is still the cleanest way
to introduce the heap. Build a small lambda application:

```wl
identity = TLam[x, x];
applied  = TApp[identity, TLam[y, y]];
TWnf[applied]
```
<!-- => the identity lambda, now reduced to TLam[y, y] in the live heap -->

`TWnf` drives a term to weak normal form. <code>[TStep]()</code> fires a single interaction and exposes the pending eliminator frames via <code>[TStack]()</code>; <code>[TRedexes]()</code> lists every redex in the live heap; <code>[TInteract]()</code> fires a named one. <code>[TCollapse]()</code> walks the SUP-tree and enumerates non-SUP leaves - the canonical way to read a superposition out as a list of choices.

Render the heap as you build:

```wl
THeapGraph[applied]
```
<!-- => a small Graph with one APP node feeding two LAM nodes -->

Per [feedback_wl_dark_mode_colors](../THVMLink-Dev.nb), the renderer adapts to light or dark themes automatically through `Style.wl`'s standard colors.

## Tensors and the UOp graph

`TTensorCreate` wraps a `NumericArray`, a `PackedArray`, or a nested list into a
fresh `TTerm` carrying a `TAG_TEN`. The buffer is shared zero-copy on CPU.

```wl
x = TTensorCreate[Range[1., 4.]];
y = TTensorCreate[Range[10., 40., 10.]];
{TTensorShape[x], TTensorDType[x]}
```
<!-- => {{4}, "f32"} -->

The `TUOp*` family builds the tensor compute graph. The graph stays lazy until
`TRealize` schedules and dispatches it:

```wl
z = TUOpReduce[ TUOpMul[ TUOpAdd[x, y], TUOpAdd[x, y] ], 0, "SUM" ];
TTensorData @ TRealize[z]
```
<!-- => NumericArray[{(1+10)^2 + (2+20)^2 + (3+30)^2 + (4+40)^2 = 3630.}, "Real32"] -->

`TMaterialize` runs the schedule without dispatching - useful for inspecting
the kernels the planner produces before any compute happens:

```wl
TMaterialize[z];
TKernelCount[]
```
<!-- => 1 (the elementwise add+mul+reduce was fused into one kernel) -->

## Lazy lambdas that compile to kernels

A lambda whose body is a UOp graph JIT-materializes into a `UOP_KERNEL` on its
first application. The compile-once, dispatch-many idiom is the basic shape of
every training loop:

```wl
sq = TLam[w, TUOpMul[w, w]];
TRealize @ TApp[sq, TTensorCreate[Range[1., 5.]]];
TKernel[1]["DispatchKind"]
```
<!-- => "jit" -->

The kernel's source is queryable as a property:

```wl
StringTake[ TKernel[1]["Source"], UpTo[120] ]
```
<!-- => the rendered C99 elementwise body -->

## Autodiff over the heap

Build a loss, fire `TGrad` (it auto-grads every float leaf), and read the
accumulated gradient with `TGradOf`:

```wl
W   = TGlorot[{4}];
xs  = TTensorCreate[{1., 2., 3., 4.}];
ys  = TTensorCreate[{1.}];
preds = TUOpReduce[ TUOpMul[W, xs], 0, "SUM" ];
loss  = TL2Loss[ TUOpAdd[preds, TUOpNeg[ys]] ];
TClearGrad[W];
TRealize @ TGrad[loss];
TTensorData @ TRealize @ TGradOf[W]
```
<!-- => the linear-regression gradient -->

`TGrad` accumulates into `TenDesc.grad`. The multi-target form returns the
gradients in one shared backward walk: `TGrad[loss, {W1, W2, ...}]`.

A single SGD step writes the update back through `TSet`:

```wl
lr = TUOpConst[-0.05, "f32"];
TSet[W, TUOpAdd[W, TUOpMul[lr, TGradOf[W]]]]
```
<!-- => the parameter updated in place; the original W TTerm still refers to the same TenDesc -->

## Plans, profiles, autotune

After the loss has materialized, the planner can project the schedule:

```wl
TMaterialize[loss];
TMemoryPlanReport @ TMemoryPlan[]
```
<!-- => a Column with top-5 largest bufs by nbytes, top-5 longest-lived by span, etc. -->

`TMemoryPlanGantt[TMemoryPlan[]]` renders the buffer life cycles as a Gantt;
`TScheduleGraph[]` renders the kernel DAG.

The kernel introspection surface lives on `TKernel`. The autotuner benchmarks
every `TKernelProposed` candidate and applies the winner:

```wl
TKernelAutotuneUnique[]
```
<!-- => <|kid -> TKernelOpts[<|"Applied" -> {TOpt[...], ...}|>], ...|> -->

Hot-path counters (`THotCountersDelta`) and `TBench` close the loop on
wallclock + memory, in case the wins do not surface in static FLOPS.

## Snapshots: shipping a runtime as data

`TContextSnapshot` captures the live heap, book cells, tensors, definitions,
and ALO state into a portable Wolfram expression. `TInitialize` restores it
into a fresh kernel.

```wl
#| eval: False
snap = TContextSnapshot @ loss;
TFree[]; TInit[];
TInitialize[snap]
```
<!-- => loss, live again, in a freshly initialized kernel -->

This is the recommended way to ship a model + training state to a different
machine or to a Wolfram Cloud session.

## Theorem proving

The `THVMLink`ATP`` context wraps `thvm`'s equational saturation engine. The
returned object is a real `ProofObject` and supports the full Wolfram property
surface:

```wl
Needs["THVMLink`ATP`"];
TFindProof[
    Inactive[Equal][x \[CircleTimes] y \[CircleTimes] z, z \[CircleTimes] y \[CircleTimes] x],
    "AbelianGroupAxioms",
    TimeConstraint -> 10
]
```
<!-- => ProofObject[<|"Theorem" -> ..., "Status" -> "Proved", ...|>] -->

`TFindProof` accepts `Method -> Automatic | "Portfolio" | "Waldmeister" |
"VampireUEQ" | "Twee" | "EProver" | "VampirePortfolio"` plus a fully
sub-optioned single-config form (`{"Completion", "Ordering" -> "LPO", ...}`).
`TAtpSchedule` previews the schedule a `Method` will expand to; `TRelevantAxioms`
exposes the relevance filter's keep / drop partition.

## Where to go next

- The [THVMLink guide](paclet:WolframInstitute/THVMLink/guide/THVMLink) lists every public symbol grouped by subsystem.
- Worked examples live under `wl/Examples/` in the source tree - `beautiful-mnist`, `gpt2`, `mlp-mnist`, `linear-train` are the end-to-end demos; `kernel-introspect.wls` and `metal-gemm-autotune.wls` are the introspection-heavy scripts.
- The Kernel/README ([wl/THVMLink/Kernel/README.md](../../Kernel/README.md)) maps the public API to its owning source file, so you know which `.wl` to read when you want to follow a symbol back to its implementation.
- The Wolfram-language style guide ([wl/GUIDE.md](../../../GUIDE.md)) carries the conventions every `.wl`, `.wls`, and `.wlt` file in the paclet follows.
