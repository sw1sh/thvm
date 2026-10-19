---
Template: Symbol
Name: TKernelFlops
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TKernelFlops
Keywords: [kernel, flops, operation count, cost, estimate, profile]
SeeAlso: [TKernel, TKernelSource, TKernelProposed, TMaterialize, TKernelCount]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TKernelFlops]()[*kid*]</code> gives an estimate of the number of arithmetic operations in one dispatch of the kernel with id *kid*.

<!-- #| annotation: 26.10.19: The estimate is a static sum over the kernel's lifted UOp DAG (kernel_flops on the C side): one flop per element for each elementwise ALU op and one per source element for a reduction, zero for movement and loads. It counts the ops of the lowered graph, so Exp (lowered to EXP2 of a product) counts 2 per element and TMaximum (a TWhere over a CMPLT mask) counts 7. -->

## Details & Options

- Each elementwise arithmetic operation counts once per output element, and a reduction counts once per element it reads. Movement, such as a reshape or a transpose, and loads count nothing.
- The estimate is computed from the kernel's graph, so it is available before the kernel runs and does not depend on the machine.
- The count is taken over the graph as the runtime lowers it, so a function built from several primitive operations counts all of them.
- <code>[TKernelFlops]()[*kid*]</code> equals <code>[TKernel]()[*kid*]["Flops"]</code>.
- For an id with no kernel, it gives 0.

## Basic Examples

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

Eight multiplications and eight additions:

```wl
TKernelFlops[kid]
```

<!-- => 16 -->

## Scope

A tensor:

```wl
x = TTensorCreate[Range[1., 8.]]
```

<!-- => a TTensor summary box: shape 8, dtype f32 -->

An elementwise square is one operation per element:

```wl
TKernelFlops[TKernel[TMaterialize[x^2]]["Kid"]]
```

<!-- => 8 -->

$x^2 + x$ fuses into one kernel of two operations per element:

```wl
TKernelFlops[TKernel[TMaterialize[x*x + x]]["Kid"]]
```

<!-- => 16 -->

A sum reads every element once:

```wl
TKernelFlops[TKernel[TMaterialize[Total[x]]]["Kid"]]
```

<!-- => 8 -->

---

A 4 x 4 matrix:

```wl
m = TTensorCreate[ArrayReshape[Range[1., 16.], {4, 4}]]
```

<!-- => a TTensor summary box: shape 4 x 4, dtype f32 -->

Its square has $4^3$ multiplications and $4^3$ additions:

```wl
TKernelFlops[TKernel[TMaterialize[m . m]]["Kid"]]
```

<!-- => 128 -->

## Applications

The estimate for the product of two $n \times n$ matrices grows as $2 n^3$:

```wl
Table[TKernelFlops[TKernel[TMaterialize[TTensorCreate[ConstantArray[1., {n, n}]] . TTensorCreate[ConstantArray[1., {n, n}]]]]["Kid"]], {n, {2, 4, 8}}]
```

<!-- => {16, 128, 1024} -->

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

The estimate is the `"Flops"` property of the kernel object:

```wl
TKernelFlops[k["Kid"]] === k["Flops"]
```

<!-- => True -->

## Possible Issues

A tensor:

```wl
x = TTensorCreate[Range[1., 8.]]
```

<!-- => a TTensor summary box: shape 8, dtype f32 -->

[Exp]() is lowered to a power of 2 of a product, two operations per element:

```wl
TKernelFlops[TKernel[TMaterialize[Exp[x]]]["Kid"]]
```

<!-- => 16 -->

An elementwise [Max]() is lowered to a comparison and a select, seven operations per element:

```wl
TKernelFlops[TKernel[TMaterialize[Max[x, 0]]]["Kid"]]
```

<!-- => 56 -->

---

An id past the newest kernel gives 0:

```wl
TKernelFlops[TKernelCount[] + 100]
```

<!-- => 0 -->
