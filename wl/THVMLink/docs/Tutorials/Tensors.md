---
Template: TechNote
Name: Tensors
Title: Tensors, Gradients and Kernels with THVMLink
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/tutorial/Tensors
Keywords: [tensor, UOp, lazy graph, automatic differentiation, gradient, kernel, code generation, fusion, JIT, TJit, memory plan, schedule graph]
RelatedGuides: [THVMLink]
RelatedTutorials: [Overview, Train, Gpt2]
---

A tensor in THVMLink is a [TTerm](), a cell of the same heap that holds lambda terms. Arithmetic on tensors builds a lazy graph of micro-operations (UOps) and computes nothing. [TRealize]() schedules the graph into kernels, fusing the operations it can, compiles each kernel to native code and runs it; [Normal]() reads a computed tensor back as a list. [TGrad]() differentiates a lazy graph by adding its backward branch to the heap. The sections below follow small computations from a list of numbers to a compiled kernel, a recorded step, a memory plan and a gradient.

## Tensors and the graph of micro-operations

[TTensorCreate]() makes a tensor from a list of real numbers. Its summary box shows its shape and dtype:

```wl
x = TTensorCreate[Range[1., 6.]]
```

[TTensorShape]() gives the shape without computing anything:

```wl
TTensorShape[x]
```
<!-- => {6} -->

[TTensorDType]() gives the element type, 32-bit floating point by default:

```wl
TTensorDType[x]
```
<!-- => "f32" -->

A nested list gives a tensor of higher rank:

```wl
m = TTensorCreate[{{1., 2., 3.}, {4., 5., 6.}}]
```

Its shape:

```wl
TTensorShape[m]
```
<!-- => {2, 3} -->

The graph is made of UOp nodes, each with its own constructor. [TUOpMul]() is an elementwise product and [TUOpReduce]() reduces along an axis, counted from 0, by `"SUM"` or `"MAX"`. Their composition is the sum of squares, which [TRealize]() computes and [Normal]() reads back:

```wl
Normal @ TRealize @ TUOpReduce[TUOpMul[x, x], 0, "SUM"]
```
<!-- => {91.} -->

Movement nodes change how the data is viewed without copying it. [TUOpReshape]() reads the same six elements in a new shape:

```wl
Normal @ TRealize @ TUOpReshape[m, {3, 2}]
```
<!-- => {{1., 2.}, {3., 4.}, {5., 6.}} -->

[TUOpPermute]() reorders the axes:

```wl
Normal @ TRealize @ TUOpPermute[m, {1, 0}]
```
<!-- => {{1., 4.}, {2., 5.}, {3., 6.}} -->

## Tensors as Wolfram Language arrays

A [TTerm]() tensor takes the ordinary Wolfram Language operations on arrays, and each builds the same UOp graph as the constructors. A vector:

```wl
a = TTensorCreate[{1., 2., 3.}]
```

A second vector:

```wl
b = TTensorCreate[{4., 5., 6.}]
```

[Plus]() adds them elementwise:

```wl
Normal @ TRealize[a + b]
```
<!-- => {5., 7., 9.} -->

[Times]() multiplies them elementwise:

```wl
Normal @ TRealize[a*b]
```
<!-- => {4., 10., 18.} -->

With [Power]() and [Total](), the sum of squares from the previous section is <code>[Total]()[*a*^2]</code>:

```wl
Normal @ TRealize[Total[a^2]]
```
<!-- => {14.} -->

A matrix:

```wl
mm = TTensorCreate[{{1., 2., 3.}, {4., 5., 6.}}]
```

[ArrayReduce]() reduces chosen axes. Reducing axis 2 with [Total]() gives the sum of each row:

```wl
Normal @ TRealize @ ArrayReduce[Total, mm, 2]
```
<!-- => {6., 15.} -->

A square matrix:

```wl
m1 = TTensorCreate[{{1., 2.}, {3., 4.}}]
```

A second square matrix:

```wl
m2 = TTensorCreate[{{5., 6.}, {7., 8.}}]
```

[Dot]() gives their matrix product:

```wl
Normal @ TRealize[m1 . m2]
```
<!-- => {{19., 22.}, {43., 50.}} -->

[Transpose]() permutes the axes:

```wl
Normal @ TRealize @ Transpose[m1]
```
<!-- => {{1., 3.}, {2., 4.}} -->

The sections below write every graph this way.

## Drawing the graph

[THeapGraph]() draws a term as an interaction net. A vector:

```wl
av = TTensorCreate[{1., 2., 3.}]
```

Its square, drawn before anything is scheduled, is a multiplication node whose two inputs are the same tensor leaf:

```wl
THeapGraph[av^2, ImageSize -> {200, 200}]
```

## Compiling kernels

[TRealize]() schedules a graph into kernels, and the central processing unit (CPU) back end compiles each new kernel to native code before running it. [TMaterialize]() stops after scheduling, so a kernel can be inspected before it runs. A vector of 8 elements:

```wl
v = TTensorCreate[Range[1., 8.]]
```

Schedule an expression whose operations fuse into one kernel. [TKernel]() wraps the scheduled kernel as an object, and its summary box shows the kernel id, the number of inputs, and the size and dtype of the output:

```wl
k = TKernel[TMaterialize[Total[(v + v)^2]]]
```

The `"Flops"` property estimates the arithmetic operations in one dispatch, as [TKernelFlops]() does. For each of the 8 elements it counts the addition `v + v` in each of the two factors of the square, the multiplication and one step of the sum:

```wl
k["Flops"]
```
<!-- => 32 -->

[TKernelProposed]() lists the changes to the kernel's loop schedule that an autotuner would try, here unrolling the loop over the 8 elements by 8, 4 or 2. Each is an optimization object with its own summary box, and [TKernelApplyOpt]() applies one:

```wl
TKernelProposed[k["Kid"]]
```

Applying the kernel object to no arguments compiles and dispatches it, and gives the tensor it writes:

```wl
Normal[k[]]
```
<!-- => {816.} -->

The `"DispatchKind"` is `"jit"`: the kernel ran as natively compiled code. This compilation happens for every new kernel, and is separate from [TJit](), described below:

```wl
k["DispatchKind"]
```
<!-- => "jit" -->

The kernel has been dispatched once:

```wl
k["DispatchCount"]
```
<!-- => 1 -->

[TKernelSource]() gives the code the kernel compiled from. On the CPU back end it is C, and the addition, the square and the sum are one loop with no intermediate buffer:

```wl
TKernelSource[k["Kid"], "C"]
```

The same kernel renders as Metal Shading Language source. Rendering the source is code generation only and needs no graphics processor (GPU):

```wl
TKernelSource[k["Kid"], "Metal"]
```

## Recording a step with TJit

[TJit]() works one level above the kernel: it records the whole sequence of kernel dispatches that a function makes, so that later applications replay the recorded kernels without building or scheduling the graph again. A tensor for the recorded function to read:

```wl
wt = TTensorCreate[{1., 2., 3.}]
```

A closure of no arguments that computes the sum of squares of `wt`. Before its first application it has recorded nothing:

```wl
step = TJit[Function[{}, TRealize @ Total[wt^2]]]
```

The first application records the dispatches and gives the result tensor:

```wl
out = step[]
```

The sum of squares of `wt`:

```wl
Normal[out]
```
<!-- => {14.} -->

The closure now shows one recorded dispatch:

```wl
step
```

[TSet]() writes new values into the buffer of `wt` in place:

```wl
TSet[wt, TTensorCreate[{2., 3., 4.}]]
```

Applying the closure again replays the recorded kernel on the new values and rewrites the same result tensor:

```wl
step[]
```

The result now holds the new sum of squares:

```wl
Normal[out]
```
<!-- => {29.} -->

A training loop runs this way: the parameters are fixed tensors, the step is recorded once, and every later step is a replay over the updated weights. The [Train](paclet:WolframInstitute/THVMLink/tutorial/Train) tutorial builds such a loop.

## Schedules and memory

[TScheduleGraph]() and [TMemoryPlan]() describe every kernel scheduled so far. Start from an empty heap with [TReset]() so that they cover only the next schedule:

```wl
TReset[];
```

A vector of 64 elements:

```wl
big = TTensorCreate[Range[1., 64.]]
```

[TMaterialize]() schedules a product of two sums into kernels without running them:

```wl
TMaterialize[Total[big^2]*Total[big]]
```

[TScheduleGraph]() draws the kernels, with an edge from each tensor or kernel to the kernels that read it:

```wl
TScheduleGraph[]
```

[TMemoryPlan]() gives a snapshot of the kernels and buffers, with the depths in the kernel graph over which each buffer is in use. Its summary box shows the numbers of kernels and buffers:

```wl
TMemoryPlan[]
```

[TMemoryPlanReport]() summarizes the plan as text, listing the largest and longest-lived buffers and the peak memory in use:

```wl
TMemoryPlanReport[TMemoryPlan[]]
```

## Gradients

[TGrad]() differentiates a scalar with respect to every floating-point tensor it depends on, and adds each gradient to that tensor's gradient slot. It works on the lazy graph: it adds the backward branch to the heap, with no separate tape and no flag marking which tensors take gradients. [TGradOf]() gives the gradient held for a tensor as a lazy graph, which [TRealize]() computes. A tensor:

```wl
W = TTensorCreate[{1., 2., 3.}]
```

Differentiate the sum of its squares:

```wl
TGrad[Total[W^2]]
```

The gradient held for `W`, which for <code>[Total]()[*W*^2]</code> is 2 *W*:

```wl
Normal @ TRealize @ TGradOf[W]
```
<!-- => {2., 4., 6.} -->

The graph must still be lazy when [TGrad]() walks it: a subexpression realized first is a buffer, and the backward branch cannot pass through it. A one-layer linear regression shows the shape of a training step. The weights:

```wl
weights = TTensorCreate[{0.1, 0.2, 0.3, 0.4}]
```

The input:

```wl
xs = TTensorCreate[{1., 2., 3., 4.}]
```

The target:

```wl
tgt = TTensorCreate[{1.}]
```

The residual of the prediction, which is 3, against the target 1:

```wl
err = Total[weights*xs] - tgt
```

Differentiate the squared error:

```wl
TGrad[err^2]
```

The gradient is twice the residual, 2, times the input:

```wl
Normal @ TRealize @ TGradOf[weights]
```
<!-- => {4., 8., 12., 16.} -->

With a list of tensors, [TGrad]() differentiates with respect to each of them in one backward pass. For the dot product <code>[Total]()[*A* *B*]</code>, the gradient with respect to each factor is the other factor. The first factor:

```wl
A = TTensorCreate[{2., 3.}]
```

The second factor:

```wl
B = TTensorCreate[{5., 7.}]
```

Differentiate with respect to both:

```wl
TGrad[Total[A*B], {A, B}]
```

The gradient with respect to `A` is `B`:

```wl
Normal @ TRealize @ TGradOf[A]
```
<!-- => {5., 7.} -->

The gradient with respect to `B` is `A`, from the same backward pass:

```wl
Normal @ TRealize @ TGradOf[B]
```
<!-- => {2., 3.} -->

A gradient-descent step computes the gradient first and then writes the update into the buffer of `A` with [TSet]():

```wl
gA = TRealize @ TGradOf[A]
```

The step with learning rate 0.1:

```wl
TSet[A, A - 0.1*gA]
```

`A` now holds the stepped values, $\{2, 3\} - 0.1 \{5, 7\}$:

```wl
Normal[A]
```
<!-- => {1.5, 2.3} -->
