---
Template: Paclet
ResourceType: Paclet
Name: WolframInstitute/THVMLink
Context: WolframInstitute`THVMLink`
ContextPath: [WolframInstitute`THVMLink`ATP`]
Paclet: WolframInstitute/THVMLink
Description: Lambda terms, lazy tensor graphs compiled to kernels, gradients and equational theorem proving on an interaction-net runtime
ContributedBy: Wolfram Institute
Keywords: [interaction net, lambda calculus, superposition, tensor, lazy graph, kernel, automatic differentiation, gradient, memory plan, theorem proving, equational logic]
MainGuide: Documentation/English/Guides/THVMLink.nb
License: MIT
WolframVersion: 13.0+
Categories: [Machine Learning, Symbolic & Numeric Computation]
SourceControlURL: https://github.com/sw1sh/thvm
---

## Details & Options

- THVMLink connects the Wolfram Language to `thvm`, a runtime that computes by rewriting an interaction net: a graph of agents joined by wires, reduced by local rules. Lambda terms, lazy tensor expressions and the compiled kernels that compute them are all cells of one heap.
- <code>[TLam]()</code>, <code>[TApp]()</code>, <code>[TSup]()</code> and <code>[TDup]()</code> build lambda terms, superpositions and duplications. <code>[TWnf]()</code> reduces a term to weak normal form, and <code>[TCollapse]()</code> lists the values a superposition stands for.
- <code>[TTensorCreate]()</code> makes a tensor from a list or a [NumericArray](). Arithmetic and array operations such as [Plus](), [Dot]() and [Total]() on tensors build a lazy graph of micro-operations, which <code>[TRealize]()</code> schedules into kernels, compiles to native code and runs, on the central processing unit (CPU) by default.
- <code>[TGrad]()</code> differentiates a scalar with respect to every floating-point tensor it depends on by adding a backward branch to the graph, and <code>[TGradOf]()</code> gives the gradient held for a tensor. <code>[TJit]()</code> records the kernels a function dispatches and replays them without building the graph again.
- <code>[THeapGraph]()</code> draws a term as an interaction net, <code>[TKernel]()</code> gives the properties of a compiled kernel, such as its source code, and <code>[TScheduleGraph]()</code> and <code>[TMemoryPlan]()</code> describe the kernels and buffers of the current schedule.
- <code>[TContextSnapshot]()</code> records the runtime as an ordinary expression, and <code>[TInitialize]()</code> restores it.
- The `` WolframInstitute`THVMLink`ATP` `` context holds the automated theorem proving functions. <code>[TFindProof]()</code> proves an equational theorem from axioms and gives a [ProofObject](), as [FindEquationalProof]() does.

## Usage

THVMLink builds terms with [TLam](), [TApp]() and [TSup]() and reduces them with [TWnf]() and [TCollapse](); makes tensors with [TTensorCreate]() and computes their lazy graphs with [TRealize](); differentiates with [TGrad]() and [TGradOf](); inspects kernels and schedules with [TKernel](), [TScheduleGraph]() and [TMemoryPlan](); saves and restores the runtime with [TContextSnapshot]() and [TInitialize](); and proves equational theorems with [TFindProof]().

## Basic Examples

Reduce the application of a lambda that adds 1 to its argument:

```wl
result = TWnf[TApp[TLam[n, n + 1], TNum[41]]]
```

The value of the result:

```wl
TTermVal[result]
```
<!-- => 42 -->

---

A tensor:

```wl
x = TTensorCreate[{1., 2., 3., 4.}]
```

A second tensor:

```wl
y = TTensorCreate[{10., 20., 30., 40.}]
```

Their sum is a lazy graph, which [TRealize]() computes and [Normal]() reads back:

```wl
Normal @ TRealize[x + y]
```
<!-- => {11., 22., 33., 44.} -->

---

A weight vector:

```wl
w = TTensorCreate[{1., 2., 3.}]
```

An input:

```wl
x = TTensorCreate[{4., 5., 6.}]
```

Differentiate the dot product of the two:

```wl
TGrad[Total[w*x]]
```

The gradient with respect to the weights is the input:

```wl
Normal @ TRealize @ TGradOf[w]
```
<!-- => {4., 5., 6.} -->

## Scope

The interaction net of the application of the successor function to 41, before it is reduced:

```wl
THeapGraph[TLam[n, n + 1][TNum[41]], ImageSize -> 300]
```

---

Start from an empty heap, so that the snapshot below holds only one lambda:

```wl
TReset[];
```

A snapshot of the successor function, an ordinary expression:

```wl
snap = TContextSnapshot[TLam[n, n + 1]]
```
<!-- => TContext[<|"Root" -> Term["LAM", 0, 1], "Cells" -> <|1 -> Term["OP2", "+", 2], 2 -> Term["VAR", 0, 1], 3 -> Term["NUM", "i32", 1]|>, "BookCells" -> <||>, "Tensors" -> <||>, "Defs" -> <||>, "AloStates" -> {}, "Labels" -> 1, "State" -> "Initialized"|>] -->

Restore the snapshot and apply the restored lambda to 9:

```wl
TTermVal @ TWnf[TInitialize[snap][TNum[9]]]
```
<!-- => 10 -->

## Applications

Start from an empty heap, so that the plan below covers only one schedule:

```wl
TReset[];
```

An input matrix:

```wl
x = TTensorCreate[ConstantArray[0.5, {16, 16}]]
```

A weight matrix:

```wl
w = TTensorCreate[ConstantArray[0.25, {16, 4}]]
```

Schedule one layer and a sum over its output, without running it:

```wl
TMaterialize[Total[Tanh[x . w], All]]
```

A summary of the buffers of the schedule:

```wl
TMemoryPlanReport[TMemoryPlan[]]
```

---

Prove that in an abelian group, written with [CircleTimes]() and [OverBar]() for the inverse, the inverse of a product is the product of the inverses:

```wl
TFindProof[ForAll[{a, b}, OverBar[a \[CircleTimes] b] == OverBar[a] \[CircleTimes] OverBar[b]], "AbelianGroupAxioms", TimeConstraint -> 10]
```

## Properties and Relations

A tensor:

```wl
v = TTensorCreate[Range[1., 8.]]
```

Schedule the sum of its squares as a kernel, without running it:

```wl
k = TKernel[TMaterialize[Total[v^2]]]
```

The candidate optimizations of its loop schedule, unrolling the loop over the 8 elements by 8, 4 or 2:

```wl
TKernelProposed[k["Kid"]]
```

Dispatch the kernel:

```wl
Normal[k[]]
```
<!-- => {204.} -->

The kernel ran as natively compiled code:

```wl
k["DispatchKind"]
```
<!-- => "jit" -->

## Possible Issues

Reduction rewrites the heap in place and consumes the term it reduces. An application:

```wl
t = TApp[TLam[n, n + 1], TNum[41]]
```

The first reduction gives 42:

```wl
TTermVal[TWnf[t]]
```
<!-- => 42 -->

Reducing the same term again reads cells that the first reduction rewrote, and does not give 42. Reduce the term that the first reduction gives instead:

```wl
TTermVal[TWnf[t]]
```
<!-- => 41 -->

## Hero Image

Agents of an interaction net wired into a stack of tensors and a neural network, under an equation:

```wl
ImageResize[Import[PacletObject["WolframInstitute/THVMLink"]["AssetLocation", "Hero"]], {1500, Automatic}]
```

## Author Notes

The documentation was written with the assistance of Claude (Anthropic). The examples are evaluated against the paclet when the documentation is built.
