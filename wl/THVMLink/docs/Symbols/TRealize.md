---
Template: Symbol
Name: TRealize
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TRealize
Keywords: [realize, evaluate, compute, dispatch, kernel, lazy graph]
SeeAlso: [TMaterialize, TTensorData, TTensorShape, TKernelCount, TGrad, Normal]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TRealize]()[*expr*]</code> computes the lazy tensor graph *expr* and gives a tensor holding its values.

<code>[TRealize]()[{$e_1$, $e_2$, …}]</code> computes several graphs in one pass and gives the list of their tensors.

<!-- #| annotation: 26.10.19: TRealize[expr] is TWnf[TMaterialize[expr]]: the materialize pass rewrites the UOp graph in place into UOP_KERNEL cells, weak-head normalization dispatches them, and on exit every CPU buffer allocated during the pass that the result does not reach is freed. On the CPU each new kernel is compiled with clang -O3 into a cached dylib on its first dispatch; that is per kernel and automatic, unlike TJit's capture and replay. The list form bundles every root into one materialize (thvm_realize_many), so a subgraph reached from several roots is emitted once, and orders the roots by data like tinygrad's scheduler: a root reading a buffer's old contents realizes before another root's TAssign overwrites it, and two roots that each overwrite what the other reads give Failure["TRealize", ...] ("cycle detected in assign graph"). TRealize of an expression that is not a TTerm gives a malformed TTerm[LibraryFunction[...][...]] instead of returning unevaluated (reported 26.10.19). -->

## Details & Options

- Arithmetic, reductions and other functions on a [TTerm]() only build a lazy graph. <code>[TRealize]()</code> schedules the graph into kernels, as <code>[TMaterialize]()</code> does, runs them and gives the result as a tensor, displayed as a summary box.
- <code>[Normal]()</code> reads the values of the result as a list, <code>[TTensorData]()</code> as a [NumericArray](), and <code>[TTensorShape]()</code> and <code>[TTensorDType]()</code> give its shape and dtype.
- On the central processing unit (CPU), a kernel is compiled to native code the first time it runs, so the first realization of a new graph takes longer than later ones.
- A tensor that already holds its values realizes to a tensor with the same values and adds no kernel.
- The list form computes every graph in one pass, so a part that several of them share is computed once.
- Each realization schedules and runs the graph again; it does not cache the values of an earlier one.

## Basic Examples

A tensor:

```wl
x = TTensorCreate[{1., 2., 3., 4.}]
```

<!-- => a TTensor summary box: shape 4, dtype f32 -->

Its elementwise square is a lazy graph until it is realized into a tensor:

```wl
r = TRealize[x*x]
```

<!-- => a TTensor summary box: shape 4, dtype f32 -->

The values of the result:

```wl
Normal[r]
```

<!-- => {1., 4., 9., 16.} -->

## Scope

A tensor:

```wl
x = TTensorCreate[{1., 2., 3., 4.}]
```

<!-- => a TTensor summary box: shape 4, dtype f32 -->

A sum of squares, a scalar:

```wl
Normal[TRealize[Total[x^2]]]
```

<!-- => {30.} -->

The values as a [NumericArray]():

```wl
TTensorData[TRealize[x*x]]
```

<!-- => NumericArray[{1., 4., 9., 16.}, "Real32"] -->

Several graphs realized together give a list of tensors:

```wl
TRealize[{x^2, x + 1}]
```

<!-- => a list of two TTensor summary boxes: shape 4, dtype f32 -->

The values of both:

```wl
Normal /@ TRealize[{x^2, x + 1}]
```

<!-- => {{1., 4., 9., 16.}, {2., 3., 4., 5.}} -->

---

A matrix:

```wl
m = TTensorCreate[{{1., 2.}, {3., 4.}}]
```

<!-- => a TTensor summary box: shape 2 x 2, dtype f32 -->

The transpose plus one:

```wl
Normal[TRealize[Transpose[m] + 1]]
```

<!-- => {{2., 4.}, {3., 5.}} -->

## Applications

A grid of points from -3 to 3:

```wl
t = TTensorCreate[Range[-3., 3., 0.25]]
```

<!-- => a TTensor summary box: shape 25, dtype f32 -->

Compute the hyperbolic tangent on the grid and plot it:

```wl
ListLinePlot[Normal[TRealize[Tanh[t]]], DataRange -> {-3, 3}, ImageSize -> 300]
```

<!-- => a line plot of tanh from -3 to 3, rising from about -1 to about 1 -->

## Properties and Relations

A tensor:

```wl
x = TTensorCreate[{1., 2., 3., 4.}]
```

<!-- => a TTensor summary box: shape 4, dtype f32 -->

Realizing is scheduling with [TMaterialize]() followed by running the kernels with [TWnf]():

```wl
Normal[TRealize[x^2 + x]] === Normal[TWnf[TMaterialize[x^2 + x]]]
```

<!-- => True -->

The count of kernels so far:

```wl
n = TKernelCount[]
```

<!-- => a positive integer -->

A tensor that already holds its values realizes with no new kernel:

```wl
Normal[TRealize[x]]
```

<!-- => {1., 2., 3., 4.} -->

No kernel was added:

```wl
TKernelCount[] - n
```

<!-- => 0 -->

## Possible Issues

A tensor:

```wl
x = TTensorCreate[{1., 2., 3., 4.}]
```

<!-- => a TTensor summary box: shape 4, dtype f32 -->

A lazy graph holds no values until it is realized:

```wl
Normal[x^2]
```

<!-- => Missing["NotATensor", "UOP"] -->

A scalar result has shape `{}` but reads back as a list of one element:

```wl
TTensorShape[TRealize[Total[x^2]]]
```

<!-- => {} -->

---

A tensor:

```wl
x = TTensorCreate[{1., 2., 3., 4.}]
```

<!-- => a TTensor summary box: shape 4, dtype f32 -->

A lazy graph:

```wl
e = x^2
```

<!-- => a TUOp summary box of kind MUL -->

The count of kernels so far:

```wl
n = TKernelCount[]
```

<!-- => a positive integer -->

Realizing the same graph twice schedules and runs it twice:

```wl
Normal[TRealize[e]] === Normal[TRealize[e]]
```

<!-- => True -->

Two kernels were added:

```wl
TKernelCount[] - n
```

<!-- => 2 -->
