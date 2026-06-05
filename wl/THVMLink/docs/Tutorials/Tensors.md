---
Template: TechNote
Name: Tensors
Title: Tensors, Autodiff, and Kernels with THVMLink
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/tutorial/Tensors
Keywords: [tensor, UOp, automatic differentiation, gradient, kernel, codegen, fusion, JIT, TJit, Metal, memory plan, schedule graph, visualization, upvalues]
RelatedGuides: [THVMLink]
RelatedTutorials: [Overview]
---

## What the tensor surface covers

The `` THVMLink` `` base context wraps `thvm`'s tensor-aware interaction-combinator runtime. Where the [ATP](paclet:WolframInstitute/THVMLink/tutorial/ATP) tutorials drive the C-side proof engines, this note walks the *other* half of the same heap: tensors, the lazy UOp compute graph, the kernels that graph compiles to, and the autodiff that runs over it. The [Overview](paclet:WolframInstitute/THVMLink/tutorial/Overview) tours the whole surface with one example each; here every section goes deep, carrying a single small computation from a raw buffer through to a compiled kernel and its gradient.

Three ideas carry the surface:

- **A tensor is a `TTerm`.** [TTensorCreate]() wraps a list / `NumericArray` / `PackedArray` into a cell carrying a `TAG_TEN`; the buffer is shared zero-copy on CPU. The `TUOp*` constructors build a lazy compute graph over those cells, and the standard Wolfram operators ([Plus]() , [Times](), [Dot](), [Total](), ...) build the *same* graph through `TTerm` UpValues.
- **Nothing runs until you ask.** The graph stays symbolic until [TRealize]() schedules it, fuses what it can, JIT-compiles each kernel, and dispatches. [TMaterialize]() runs the planner *without* dispatching, so you can inspect the schedule before any compute happens. [Normal]() reads a realized tensor back as an ordinary list.
- **The whole graph is differentiable.** Build a scalar and [TGrad]() fires one backward walk over the heap, auto-grading every float leaf and accumulating each one's gradient into its grad slot.

The notebook loads the `THVMLink`` context from its metadata and the runtime initializes on first use, so the examples below start straight in - there is no load or init step.

## Tensors and the UOp graph

[TTensorCreate]() ingests a flat or nested list of reals. The result is a `TTerm` you can query without realizing anything. Read its shape with [TTensorShape]():

```wl
x = TTensorCreate[Range[1., 6.]];
TTensorShape[x]
```
<!-- => {6} -->

and its element type with [TTensorDType]():

```wl
TTensorDType[x]
```
<!-- => "f32" -->

Nested lists become rank-2 (and higher) tensors:

```wl
m = TTensorCreate[{{1., 2., 3.}, {4., 5., 6.}}];
TTensorShape[m]
```
<!-- => {2, 3} -->

The `TUOp*` family builds the compute graph node by node. [TUOpMul]() is an elementwise product, [TUOpReduce]() contracts an axis (`"SUM"`, `"MAX"`, ...). Nothing computes until [TRealize]() drives the graph; [Normal]() then reads the buffer back as a list:

```wl
Normal @ TRealize @ TUOpReduce[TUOpMul[x, x], 0, "SUM"]
```
<!-- => {91.}  (1^2 + 2^2 + ... + 6^2) -->

Movement operators reshape the *view* without touching the buffer. [TUOpReshape]() reinterprets the data under a new shape:

```wl
Normal @ TRealize @ TUOpReshape[m, {3, 2}]
```
<!-- => {{1., 2.}, {3., 4.}, {5., 6.}} -->

and [TUOpPermute]() transposes axes:

```wl
Normal @ TRealize @ TUOpPermute[m, {1, 0}]
```
<!-- => {{1., 4.}, {2., 5.}, {3., 6.}} -->

## Tensors as Wolfram arrays

You rarely call the `TUOp*` constructors directly. `thvm` installs UpValues on `TTerm`, so the ordinary Wolfram operators build the *same* lazy UOp graph - a `TTerm` behaves like a packed array that happens to defer its compute. [Plus]() and [Times]() lower to the elementwise UOps:

```wl
a = TTensorCreate[{1., 2., 3.}];
b = TTensorCreate[{4., 5., 6.}];
Normal @ TRealize[a + b]
```
<!-- => {5., 7., 9.} -->

```wl
Normal @ TRealize[a*b]
```
<!-- => {4., 10., 18.} -->

[Power]() and [Total]() reduce the explicit `TUOpMul` + `TUOpReduce` of the previous section to one readable line - the sum of squares is just <code>[Total]()[*a*^2]</code>:

```wl
Normal @ TRealize[Total[a^2]]
```
<!-- => {14.} -->

[ArrayReduce]() sums chosen axes; here the columns of a matrix (axis 2):

```wl
mm = TTensorCreate[{{1., 2., 3.}, {4., 5., 6.}}];
Normal @ TRealize @ ArrayReduce[Total, mm, 2]
```
<!-- => {6., 15.}  (row-wise sums of {{1,2,3},{4,5,6}}) -->

[Dot]() lowers to a fused matmul, and [Transpose]() to a permute:

```wl
m1 = TTensorCreate[{{1., 2.}, {3., 4.}}];
m2 = TTensorCreate[{{5., 6.}, {7., 8.}}];
Normal @ TRealize[m1 . m2]
```
<!-- => {{19., 22.}, {43., 50.}} -->

```wl
Normal @ TRealize @ Transpose[m1]
```
<!-- => {{1., 3.}, {2., 4.}} -->

Each of these is lazy, fused, and differentiable exactly like the explicit `TUOp*` form - it *is* that graph after the UpValue fires. Every section from here on is written this way.

## Visualizing the graph

The runtime is one symbolic heap, and [THeapGraph]() renders any term as an interaction-net string diagram. Here is the squared graph - the multiply node over the shared tensor leaf - before it is scheduled:

```wl
av = TTensorCreate[{1., 2., 3.}];
THeapGraph[av^2]
```
<!-- => a Graph: one MUL node over the shared TAG_TEN leaf -->

The diagram adapts to light or dark notebook themes automatically.

## Compiling kernels: the JIT

When you [TRealize]() a graph, `thvm` schedules it into kernels and the CPU backend hands each one to `clang -O3` - so the kernel arrives JIT-compiled, with no explicit compile step. Realize a small fused expression on a concrete tensor:

```wl
x = TTensorCreate[Range[1., 8.]];
Normal @ TRealize[Total[(x + x)^2]]
```
<!-- => {816.}  (sum of (2 x)^2 for x = 1..8) -->

The whole `add -> square -> sum` expression fused into one kernel - the most recent one in the table, <code>[TKernelCount]()[] - 1</code>. [TKernel]() is a typed handle onto it; its summary box shows the kid, output shape and dtype, input count, and whether it has fired:

```wl
kid = TKernelCount[] - 1;
TKernel[kid]
```
<!-- => TKernel summary box: kid, out shape/dtype, inputs, fired -->

Its `"DispatchKind"` is `"jit"` - the clang path, taken *automatically* the moment it was realized (no [TJit]() call; that is the separate capture mechanism below):

```wl
TKernel[kid]["DispatchKind"]
```
<!-- => "jit" -->

[TKernelSource]() shows what it compiled to. On the CPU backend that is C99, and the add, square, and reduce have collapsed into one loop with no intermediate buffers:

```wl
TKernelSource[kid, "C"]
```
<!-- =>
void k(void *out_v, const void *const *ins_v, unsigned n, const unsigned *in_numels) {
  ...
  float _acc0 = 0.0f;
  for (uint a0 = 0; a0 < 8; a0++) /*reduce*/ {
    _acc0 = _acc0 + ((in0[a0] + in0[a0]) * (in0[a0] + in0[a0]));
  }
  out[0] = _acc0;
}
-->

The same lifted graph renders to Metal Shading Language - pure codegen, no GPU dispatch, so it works with no Metal device present:

```wl
StringTake[TKernelSource[kid, "Metal"], UpTo[120]]
```
<!-- => "#include <metal_stdlib>\nusing namespace metal;\n\nkernel void k(\n    device float *out [[ buffer(0) ]], ..." -->

The handle carries a profiling surface too - the static FLOP estimate with [TKernelFlops]():

```wl
TKernel[kid]["Flops"]
```
<!-- => 32 -->

and the live dispatch counter (the kernel has fired once):

```wl
TKernel[kid]["DispatchCount"]
```
<!-- => 1 -->

[TKernelProposed]() lists the schedule optimizations the autotuner would try - UNROLL / UPCAST / LOCAL splits keyed by the kernel's shape, applied with [TKernelApplyOpt]() (or benched as a set under `BEAM > 0`). Each is a [TOpt]() object that renders as its own summary box:

```wl
TKernelProposed[kid]
```
<!-- => {TOpt[UNROLL, 0, 8], TOpt[UNROLL, 0, 4], TOpt[UNROLL, 0, 2]} (each a TOpt summary box) -->

## Capturing a step with TJit

The `"jit"` above is *per-kernel*: each kernel is clang-compiled once and cached. [TJit]() works one level up - it captures a whole *dispatch sequence* (a training step's worth of kernels and in-place writes) into a closure that replays without re-scheduling. Wrap a no-argument function that closes over the buffers it reads; the first call captures and returns the result:

```wl
w = TTensorCreate[{1., 2., 3.}];
step = TJit[Function[{}, TRealize @ Total[w^2]]]
```
<!-- => TJitClosure summary box: captured: no, ops: 0 -->

The closure starts *uncaptured* (gray play-button). Its first call captures the dispatch sequence and returns the result:

```wl
out = step[];
Normal[out]
```
<!-- => {14.}  (1 + 4 + 9) -->

[TJitOpCount]() reports how many dispatches were captured:

```wl
TJitOpCount[step]
```
<!-- => 1 -->

Now mutate the input buffer in place with [TSet]() and call again. The replay re-fires the captured kernel on the new data and updates `out` in place - no re-materialize, no re-schedule:

```wl
TSet[w, TTensorCreate[{2., 3., 4.}]];
step[];
Normal[out]
```
<!-- => {29.}  (4 + 9 + 16) -->

This is exactly how a training loop runs: the parameters are fixed `TTerm`s, the step is captured once, and every epoch is a replay over mutated weights.

## Scheduling and memory

By now the runtime has compiled every expression above into kernels. [TMaterialize]() runs the planner over the current graph without dispatching, and [TScheduleGraph]() renders the kernel-dependency DAG it produces across the whole live schedule:

```wl
a = TTensorCreate[Range[1., 64.]];
TMaterialize[Total[a^2]];
TScheduleGraph[]
```
<!-- => a Graph: the kernel dependency DAG of the live schedule -->

[TMemoryPlan]() projects the buffer schedule - one record per kernel and per live buffer, with alive-spans and a peak-concurrent-bytes figure. Its summary box reports the kernel and buffer counts, total live bytes, and the active backend:

```wl
TMemoryPlan[]
```
<!-- => TMemoryPlan summary box: kernels, bufs, live bytes, backend -->

[TMemoryPlanReport]() formats it as a top-N summary (largest buffers, longest-lived, peak):

```wl
TMemoryPlanReport[TMemoryPlan[]]
```
<!-- => a Column summarizing the plan's largest / longest-lived buffers and peak -->

and [TMemoryPlanGantt]() draws each buffer's life cycle as a Gantt chart:

```wl
TMemoryPlanGantt[TMemoryPlan[]]
```
<!-- => a Gantt chart of buffer alive-spans across schedule depth -->

## Automatic differentiation

Autodiff is just more of the heap. Build a scalar with the ordinary operators and [TGrad]() fires one backward walk seeded with ones at the scalar - it *adds the backward branch to the graph* and auto-grads every reachable float leaf, accumulating each one's gradient into its grad slot. There is no `requires_grad` flag (matching tinygrad); it does not run a separate forward or keep a tape. [TGradOf]() reads the accumulated gradient term back, which you realize to compute. The gradient of <code>[Total]()[*W*^2]</code> is `2 W`:

```wl
W = TTensorCreate[{1., 2., 3.}];
TGrad[Total[W^2]];
Normal @ TRealize @ TGradOf[W]
```
<!-- => {2., 4., 6.}  (= 2 W) -->

The forward must still be a live lazy graph when `TGrad` walks it - realizing a forward subexpression first collapses it to a buffer, and the backward can no longer thread the cotangent through it. A one-layer linear regression makes the shape concrete: with `W = {0.1, 0.2, 0.3, 0.4}` against input `{1, 2, 3, 4}` the prediction <code>[Total]()[*W* *x*]</code> is `3.0`, the residual against a target of `1.0` is `2.0`, and the squared-error gradient is *2 (residual) x*:

```wl
W   = TTensorCreate[{0.1, 0.2, 0.3, 0.4}];
xs  = TTensorCreate[{1., 2., 3., 4.}];
tgt = TTensorCreate[{1.}];
err = Total[W*xs] - tgt;
TGrad[err^2];
Normal @ TRealize @ TGradOf[W]
```
<!-- => {4., 8., 12., 16.}  (= 2 * 2 * {1, 2, 3, 4}) -->

The multi-target form differentiates several leaves in one backward walk. For the dot product <code>[Total]()[*A* *B*]</code> the gradient with respect to each factor is the other - here is `d/dA`:

```wl
A = TTensorCreate[{2., 3.}];
B = TTensorCreate[{5., 7.}];
TGrad[Total[A*B], {A, B}];
Normal @ TRealize @ TGradOf[A]
```
<!-- => {5., 7.}  (= B) -->

and `d/dB`, read from the same backward pass:

```wl
Normal @ TRealize @ TGradOf[B]
```
<!-- => {2., 3.}  (= A) -->

A gradient-descent step writes the update back through [TSet](), in place - the original `TTerm` still names the same descriptor, now holding the stepped value:

```wl
TSet[A, A + (-0.1)*TGradOf[A]];
Normal @ A
```
<!-- => {1.5, 2.3}  (A - 0.1 * dA = {2, 3} - 0.1 * {5, 7}) -->

Build the scalar, [TGrad](), [TSet]() - that three-line loop is the whole of training. Wrap it in a [TJit]() closure (as above) so the step compiles once and replays every epoch; the `linear-train` and `mlp-mnist` demos under `wl/Examples/` are the worked versions.

## Where to go next

- Per-symbol pages: [TTensorCreate](paclet:WolframInstitute/THVMLink/ref/TTensorCreate), [TUOpAdd](paclet:WolframInstitute/THVMLink/ref/TUOpAdd) (representative of the whole `TUOp*` family), [TGrad](paclet:WolframInstitute/THVMLink/ref/TGrad), [TKernel](paclet:WolframInstitute/THVMLink/ref/TKernel), [TJit](paclet:WolframInstitute/THVMLink/ref/TJit), [TMemoryPlan](paclet:WolframInstitute/THVMLink/ref/TMemoryPlan), [THeapGraph](paclet:WolframInstitute/THVMLink/ref/THeapGraph).
- The [Overview](paclet:WolframInstitute/THVMLink/tutorial/Overview) tours the rest of the surface: the pure interaction-net layer, runtime snapshots via `TContextSnapshot`, and the [ATP](paclet:WolframInstitute/THVMLink/tutorial/ATP) proof engines.
- Worked end-to-end models live under `wl/Examples/` (`beautiful-mnist`, `gpt2`, `mlp-mnist`, `linear-train`); `kernel-introspect.wls` and `metal-gemm-autotune.wls` are the introspection-heavy scripts.
- The [Kernel/README](../../Kernel/README.md) maps each public symbol to its owning source file.
