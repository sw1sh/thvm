---
Template: TechNote
Name: Overview
Title: A Tour of THVMLink
Context: WolframInstitute`THVMLink`
ContextPath: [WolframInstitute`THVMLink`ATP`]
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/tutorial/Overview
Keywords: [interaction net, lambda calculus, superposition, tensor, automatic differentiation, kernel, memory plan, snapshot, theorem proving, tour]
RelatedGuides: [THVMLink]
RelatedTutorials: [Tensors, Train, Gpt2, ATP]
---

THVMLink connects the Wolfram Language to `thvm`, a runtime that computes by rewriting an interaction net: a graph of agents joined by wires, reduced by local rules that each replace a pair of connected agents. Lambda terms, lazy tensor expressions and the compiled kernels that compute them are all cells of one heap. From the Wolfram Language you build terms and tensors symbolically, reduce or compute them, differentiate tensor expressions, inspect the kernels and buffers the runtime schedules, save the runtime as an expression, and prove equational theorems.

The runtime starts on first use, on the central processing unit (CPU) back end, so the examples below begin directly.

## Terms and reduction

[TLam]() builds a lambda. This one adds 1 to its argument:

```wl
succ = TLam[n, n + 1]
```

Applying it to a number, built with [TNum](), gives an application term. Nothing is reduced yet:

```wl
app = succ[TNum[41]]
```

[THeapGraph]() draws the term as an interaction net, with one vertex for each agent and one edge for each wire:

```wl
THeapGraph[app, ImageSize -> 300]
```

[TWnf]() reduces the application to weak normal form. The result is a number term:

```wl
result = TWnf[app]
```

[TTermVal]() reads its value:

```wl
TTermVal[result]
```
<!-- => 42 -->

A superposition, built with [TSup](), stands for two values at once. Applying the successor to a superposition of 1 and 2 reduces both branches, and [TCollapse]() lists the resulting leaves:

```wl
leaves = TCollapse[TLam[n, n + 1][TSup[1, 2]]]
```

The values of the leaves:

```wl
TTermVal /@ leaves
```
<!-- => {2, 3} -->

## Tensors and the lazy graph

[TTensorCreate]() makes a tensor from a list of numbers:

```wl
xs = TTensorCreate[{1., 2., 3., 4.}]
```

A second tensor of the same shape:

```wl
ys = TTensorCreate[{10., 20., 30., 40.}]
```

[TTensorShape]() gives its shape:

```wl
TTensorShape[xs]
```
<!-- => {4} -->

Ordinary arithmetic on tensors builds a lazy graph of micro-operations (UOps) and computes nothing:

```wl
sumsq = Total[(xs + ys)^2]
```

[Term]() shows the graph that the arithmetic built, a reduction over a product of two sums of the tensor leaves:

```wl
Term[sumsq]
```
<!-- => Term["UOP", "REDUCE", Term["UOP", "MUL", Term["UOP", "ADD", Term["TEN", 1], Term["TEN", 2]], Term["UOP", "ADD", Term["TEN", 1], Term["TEN", 2]]], Term["NUM", 0], Term["NUM", 1]] -->

[TRealize]() schedules the graph into kernels, compiles them and runs them, giving a tensor that holds the result:

```wl
total = TRealize[sumsq]
```

[Normal]() reads the values back as a list:

```wl
Normal[total]
```
<!-- => {3630.} -->

## Kernels

[TMaterialize]() schedules a graph into kernels without running them, and [TKernel]() wraps the scheduled kernel as an object:

```wl
k = TKernel[TMaterialize[(xs - ys)^2]]
```

Applying the kernel object to no arguments dispatches it and gives the tensor it writes:

```wl
Normal[k[]]
```
<!-- => {81., 324., 729., 1296.} -->

The dispatch ran natively compiled code on the CPU:

```wl
k["DispatchKind"]
```
<!-- => "jit" -->

The `"Flops"` property estimates the number of arithmetic operations in one dispatch, as [TKernelFlops]() does:

```wl
k["Flops"]
```
<!-- => 20 -->

## Gradients

A weight vector to fit, so that its dot product with `xs` is 1:

```wl
w = TTensorCreate[{0.1, 0.2, 0.3, 0.4}]
```

The squared error, again a lazy graph:

```wl
loss = (Total[w*xs] - 1.)^2
```

[TGrad]() differentiates the loss with respect to every floating-point tensor it depends on and adds each gradient to that tensor's gradient slot. It gives the loss back:

```wl
TGrad[loss]
```

[TGradOf]() gives the gradient held for `w` as a lazy graph, and [TRealize]() computes it:

```wl
grad = TRealize[TGradOf[w]]
```

The prediction is 3 against a target of 1, so the gradient is $2 \cdot 2 = 4$ times `xs`:

```wl
Normal[grad]
```
<!-- => {4., 8., 12., 16.} -->

[TSet]() writes a gradient-descent step into the buffer of `w` in place:

```wl
TSet[w, w - 0.01*grad]
```

The updated weights:

```wl
Normal[w]
```
<!-- => {0.06, 0.12, 0.18, 0.24} -->

The loss after the step is lower than the 4 it started from:

```wl
Normal @ TRealize[(Total[w*xs] - 1.)^2]
```
<!-- => {0.64} -->

## Schedules and memory

[TScheduleGraph]() draws the kernels scheduled so far, with an edge from each tensor or kernel to the kernels that read it:

```wl
TScheduleGraph[]
```

[TMemoryPlan]() gives a snapshot of the kernels, tensors and buffers of the current schedule:

```wl
TMemoryPlan[]
```

[TMemoryPlanReport]() summarizes the plan as text, with the largest and longest-lived buffers and the peak memory in use:

```wl
TMemoryPlanReport[TMemoryPlan[]]
```

## Saving the runtime as an expression

[TContextSnapshot]() records the whole heap, so start from an empty one with [TReset]() to keep the snapshot small:

```wl
TReset[];
```

A snapshot of the successor function, with the lambda as its root:

```wl
snap = TContextSnapshot[TLam[n, n + 1]]
```
<!-- => TContext[<|"Root" -> Term["LAM", 0, 0], "Cells" -> <|0 -> Term["OP2", "+", 1], 1 -> Term["VAR", 0, 0], 2 -> Term["NUM", "i32", 1]|>, "BookCells" -> <||>, "Tensors" -> <||>, "Defs" -> <||>, "AloStates" -> {}, "Labels" -> 1, "State" -> "Initialized"|>] -->

The snapshot is an ordinary expression that can be stored and moved to another session. [TInitialize]() writes it back into the runtime and gives the root as a live term:

```wl
restored = TInitialize[snap]
```

The restored lambda computes as before:

```wl
TTermVal @ TWnf[restored[TNum[9]]]
```
<!-- => 10 -->

## Theorem proving

The `` WolframInstitute`THVMLink`ATP` `` context holds the automated theorem proving (ATP) functions. [TFindProof]() proves an equational theorem from a set of axioms and gives a [ProofObject](), the same kind of object [FindEquationalProof]() gives. In an abelian group, written with [CircleTimes]() for the product and [OverBar]() for the inverse, the inverse of a product is the product of the inverses:

```wl
proof = TFindProof[ForAll[{x, y}, OverBar[x \[CircleTimes] y] == OverBar[x] \[CircleTimes] OverBar[y]], "AbelianGroupAxioms", TimeConstraint -> 10]
```

The proof object answers the usual properties, such as the number of steps:

```wl
proof["ProofLength"]
```
<!-- => 19 -->
