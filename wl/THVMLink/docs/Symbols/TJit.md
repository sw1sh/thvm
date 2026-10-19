---
Template: Symbol
Name: TJit
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TJit
Keywords: [JIT, just-in-time, closure, capture, replay, dispatch]
SeeAlso: [TRealize, TSet, TKernel, Function]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TJit]()[*f*]</code> gives a closure that evaluates *f* once while recording the kernels it dispatches, and replays the recorded kernels on every later application.

<!-- #| annotation: 26.10.19: TJit mirrors tinygrad's TinyJit. The first application opens a capture slot (the C runtime keeps a pool of 16), evaluates the held function with the per-call TTerm inputs realized inside the capture scope, pins the result buffers and declares the input sites; a later application realizes its inputs and substitutes them at those sites (tinygrad's input_replace) before re-dispatching, so a replay skips materialize and scheduling and costs only the dispatches. The per-closure state lives in a side store keyed by the closure's hash, which is why a copy of a closure shares its recording. TJitDrop and TJitOpCount, which release a slot and count its dispatches, have no reference pages yet. This is separate from the CPU backend compiling each new kernel with clang, which happens on every TRealize. -->

## Details & Options

- [TJit]() has attribute [HoldFirst](). The closure it gives is a `TJitClosure` object whose summary box shows whether it has recorded and how many kernel dispatches it holds.
- The first application <code>*closure*[$x_1$, $x_2$, …]</code> evaluates <code>*f*[$x_1$, $x_2$, …]</code>, records every kernel it dispatches, and gives the result of *f*.
- A later application with new tensors of the same shapes replays the recorded dispatches on the new tensors, without building or scheduling the graph again.
- Every application gives the same result tensor, and each replay rewrites its buffer, so a result read after a later application shows the later values.
- A closure of no arguments, built from <code>[Function]()[{}, …]</code>, replays on the current contents of the tensors it recorded, which <code>[TSet]()</code> changes in place.
- The body of *f* must realize its result with <code>[TRealize]()</code>. A body that only builds a lazy graph dispatches nothing, and there is nothing to replay.
- The recording keeps the shapes of the first application. `TJitDrop[`*closure*`]` releases it, so the next application records afresh.

## Basic Examples

A closure that squares its input:

```wl
f = TJit[Function[x, TRealize[x*x]]]
```

<!-- => a TJitClosure summary box: captured: no, ops: 0 -->

The first application records the kernel it dispatches and gives the result:

```wl
Normal[f[TTensorCreate[{1., 2., 3.}]]]
```

<!-- => {1., 4., 9.} -->

The closure now holds one recorded dispatch:

```wl
f
```

<!-- => a TJitClosure summary box: captured: yes, ops: 1 -->

A later application replays the recorded kernel on the new input:

```wl
Normal[f[TTensorCreate[{4., 5., 6.}]]]
```

<!-- => {16., 25., 36.} -->

## Scope

A closure whose body dispatches two kernels, an exponential and a matrix product:

```wl
m = TJit[Function[x, TRealize[Exp[x] . Transpose[x]]]]
```

<!-- => a TJitClosure summary box: captured: no, ops: 0 -->

Apply it to the 2 x 2 identity:

```wl
Normal[m[TTensorCreate[{{1., 0.}, {0., 1.}}]]]
```

<!-- => {{2.71828, 1.}, {1., 2.71828}} -->

Both dispatches are recorded:

```wl
m
```

<!-- => a TJitClosure summary box: captured: yes, ops: 2 -->

---

A tensor that a closure of no arguments reads:

```wl
xs = TTensorCreate[{1., 2., 3., 4.}]
```

<!-- => a TTensor summary box: shape 4, dtype f32 -->

The closure:

```wl
h = TJit[Function[{}, TRealize[xs*xs]]]
```

<!-- => a TJitClosure summary box: captured: no, ops: 0 -->

Its first application records and gives the result tensor:

```wl
out = h[]
```

<!-- => a TTensor summary box: shape 4, dtype f32 -->

The result:

```wl
Normal[out]
```

<!-- => {1., 4., 9., 16.} -->

Overwrite the input in place:

```wl
TSet[xs, TTensorCreate[{10., 20., 30., 40.}]]
```

<!-- => the TTensor summary box of xs -->

Replay:

```wl
h[]
```

<!-- => the TTensor summary box of out -->

The same result tensor now holds the squares of the new contents:

```wl
Normal[out]
```

<!-- => {100., 400., 900., 1600.} -->

## Applications

A closure for the sum of squares:

```wl
g = TJit[Function[x, TRealize[Total[x^2]]]]
```

<!-- => a TJitClosure summary box: captured: no, ops: 0 -->

Evaluate it on a stream of inputs, recording on the first and replaying on the others:

```wl
Table[First[Normal[g[TTensorCreate[{i, i + 1.}]]]], {i, 1., 3.}]
```

<!-- => {5., 13., 25.} -->

## Properties and Relations

[TJit]() holds its argument unevaluated:

```wl
Attributes[TJit]
```

<!-- => {HoldFirst} -->

---

A closure that squares its input:

```wl
f = TJit[Function[x, TRealize[x*x]]]
```

<!-- => a TJitClosure summary box: captured: no, ops: 0 -->

Its first result:

```wl
r1 = f[TTensorCreate[{1., 2., 3.}]]
```

<!-- => a TTensor summary box: shape 3, dtype f32 -->

A second application gives the same result tensor:

```wl
r1 === f[TTensorCreate[{4., 5., 6.}]]
```

<!-- => True -->

So the first result now reads the second application's values:

```wl
Normal[r1]
```

<!-- => {16., 25., 36.} -->

---

A closure for the elementwise exponential:

```wl
e = TJit[Function[y, TRealize[Exp[y]]]]
```

<!-- => a TJitClosure summary box: captured: no, ops: 0 -->

Record it on one input:

```wl
Normal[e[TTensorCreate[{0., 0., 0.}]]]
```

<!-- => {1., 1., 1.} -->

Another input:

```wl
v = TTensorCreate[{1., 2., 3.}]
```

<!-- => a TTensor summary box: shape 3, dtype f32 -->

The replay on it agrees with realizing the graph without [TJit]():

```wl
Normal[e[v]] === Normal[TRealize[Exp[v]]]
```

<!-- => True -->

## Possible Issues

A body that only builds a lazy graph dispatches no kernel:

```wl
k = TJit[Function[x, x*x]]
```

<!-- => a TJitClosure summary box: captured: no, ops: 0 -->

Its application gives the lazy graph:

```wl
k[TTensorCreate[{1., 2.}]]
```

<!-- => a TUOp summary box of kind MUL -->

The recording is empty:

```wl
k
```

<!-- => a TJitClosure summary box: captured: yes, ops: 0 -->

---

A closure recorded on a one-element input:

```wl
p = TJit[Function[x, TRealize[x + 1]]]
```

<!-- => a TJitClosure summary box: captured: no, ops: 0 -->

Record it:

```wl
Normal[p[TTensorCreate[{1.}]]]
```

<!-- => {2.} -->

The replay keeps the recorded shape, so a two-element input gives a one-element result:

```wl
Normal[p[TTensorCreate[{1., 2.}]]]
```

<!-- => {2.} -->

Release the recording:

```wl
TJitDrop[p]
```

The next application records afresh at the new shape:

```wl
Normal[p[TTensorCreate[{1., 2.}]]]
```

<!-- => {2., 3.} -->
