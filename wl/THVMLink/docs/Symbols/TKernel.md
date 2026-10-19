---
Template: Symbol
Name: TKernel
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TKernel
Keywords: [kernel, codegen, dispatch, custom kernel, properties, profile]
SeeAlso: [TMaterialize, TRealize, TKernelCount, TKernelSource, TKernelFlops, TKernelProposed, TKernelApplyOpt]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TKernel]()[*t*]</code> gives the kernel object for the scheduled kernel term *t*, such as a result of <code>[TMaterialize]()</code>.

<code>[TKernel]()[*kid*]</code> gives the kernel object for the kernel with id *kid*.

<code>[TKernel]()[*spec*]</code> represents a custom kernel whose C source and launch parameters are given by the association *spec*.

<!-- #| annotation: 26.10.19: A kernel object is TKernel[<|"Term" -> t, "Kid" -> kid|>], a wrapper around the pinned UOP_KERNEL heap cell; TKernel[kid] finds that cell by scanning the heap. Properties read the C KernelEntry side table. ttermRaw, TTermVal, TTermTag and TTermExt route through the inner term, which is what lets the TUOp* constructors take a kernel object, and Plus, Times and Dot UpValues on TKernel hand the inner term to the TTerm arithmetic. The custom form registers the source with thvm_wl_kernel_emit_program once per distinct buffer binding and fires it through the backend vtable, so a fire inside a TJit body is recorded. -->

## Details & Options

- A kernel object displays as a summary box showing its id, number of inputs, output size and dtype.
- *kernel*`["`*prop*`"]` gives a property of the kernel, and <code>[Information]()[*kernel*, "Properties"]</code> lists them all. *kernel*`["Source", "`*backend*`"]` gives the source code for the backend `"C"` or `"Metal"`. Among the properties are:

  | | |
  | --- | --- |
  | `"Kid"` | the kernel id |
  | `"OutputShape"` | the shape of the tensor the kernel writes, `{}` for a scalar |
  | `"OutputDtype"` | the dtype of that tensor |
  | `"InputCount"` | the number of input tensors |
  | `"InputTensors"` | the input tensors |
  | `"Flops"` | the estimated number of arithmetic operations in one dispatch |
  | `"Source"` | the source code the kernel compiles from |
  | `"DispatchKind"` | how the last dispatch ran, such as `"jit"` for natively compiled code |
  | `"DispatchCount"` | the number of dispatches so far |
  | `"AvgUs"` | the average time of one dispatch, in microseconds |

- *kernel*`[]` dispatches the kernel and gives the tensor it writes.
- Kernel ids count up from 1 as <code>[TMaterialize]()</code> and <code>[TRealize]()</code> schedule new kernels, so the newest kernel has id <code>[TKernelCount]()[] - 1</code>.
- The graph-node constructors, such as <code>[TUOpAdd]()</code>, accept a kernel object in place of the tensor it writes, and so do the arithmetic operators `+` and `*` and [Dot]().
- In a custom *spec*, `"Program"` is C source defining `void k(void *const *bufs, unsigned const *grid, unsigned const *block)`. `"Name"` names the entry point (default `"k"`), `"Grid"` and `"Block"` give up to three launch dimensions (default `{1, 1, 1}`), and `"Outs"` lists the positions of the arguments the program writes (default [All]()).
- <code>[TKernel]()[*spec*][$b_1$, $b_2$, …]</code> realizes the tensors $b_i$, passes their buffers to the program in that order, runs it, and gives the written tensor, or the list of written tensors when there are several.
- <code>[TKernel]()[*kid*]</code> gives a [Failure]() for an id with no kernel.

## Basic Examples

A tensor:

```wl
x = TTensorCreate[Range[1., 8.]]
```

<!-- => a TTensor summary box: shape 8, dtype f32 -->

Schedule its elementwise square as one kernel, without running it, and wrap it as a kernel object:

```wl
k = TKernel[TMaterialize[x^2]]
```

<!-- => a TKernel summary box: 1 input, output numel 8, output dtype f32 -->

Dispatch the kernel and read the tensor it writes:

```wl
Normal[k[]]
```

<!-- => {1., 4., 9., 16., 25., 36., 49., 64.} -->

The kernel has been dispatched once:

```wl
k["DispatchCount"]
```

<!-- => 1 -->

## Scope

A tensor:

```wl
x = TTensorCreate[Range[1., 8.]]
```

<!-- => a TTensor summary box: shape 8, dtype f32 -->

The kernel of a sum of squares:

```wl
k = TKernel[TMaterialize[Total[x^2]]]
```

<!-- => a TKernel summary box: 1 input, output numel 1, output dtype f32 -->

Its output is a scalar:

```wl
k["OutputShape"]
```

<!-- => {} -->

It reads one tensor:

```wl
k["InputCount"]
```

<!-- => 1 -->

One dispatch performs an estimated 16 operations, 8 multiplications and 8 additions:

```wl
k["Flops"]
```

<!-- => 16 -->

Every property the kernel object has:

```wl
Information[k, "Properties"]
```

<!-- => {"Kid", "Term", "OutputTid", "OutputShape", "OutputDtype", "OutputNumel", "InputCount", "InputTids", "InputTensors", "OpCount", "Program", "ProgramKey", "Fired", "Spliced", "ConsumerCount", "Source", "JitDylibPath", "Flops", "DispatchKind", "DispatchCount", "TotalUs", "AvgUs", "GFlopsPerSec"} -->

Dispatch it:

```wl
Normal[k[]]
```

<!-- => {204.} -->

On the central processing unit (CPU), the kernel ran as natively compiled code:

```wl
k["DispatchKind"]
```

<!-- => "jit" -->

The source of the kernel for a named backend, here the Metal Shading Language:

```wl
StringStartsQ[k["Source", "Metal"], "#include <metal_stdlib>"]
```

<!-- => True -->

---

Realizing a graph compiles and runs its kernel:

```wl
TRealize[TTensorCreate[{1., 2., 3.}] + TTensorCreate[{10., 20., 30.}]]
```

<!-- => a TTensor summary box: shape 3, dtype f32 -->

The newest kernel, by its id:

```wl
TKernel[TKernelCount[] - 1]
```

<!-- => a TKernel summary box: 2 inputs, output numel 3, output dtype f32 -->

---

A custom kernel named `dbl` that doubles four floats, writing its first argument:

```wl
double = TKernel[<|"Program" -> "void dbl(void *const *bufs, unsigned const *grid, unsigned const *block) { float *o = (float *)bufs[0]; const float *a = (const float *)bufs[1]; for (int i = 0; i < 4; i++) o[i] = 2.0f * a[i]; }", "Name" -> "dbl", "Outs" -> {1}|>]
```

<!-- => TKernel[<|"Program" -> "void dbl(...) {...}", "Name" -> "dbl", "Outs" -> {1}|>] -->

A tensor for the output:

```wl
out = TTensorCreate[{0., 0., 0., 0.}]
```

<!-- => a TTensor summary box: shape 4, dtype f32 -->

Run the program on an input; it gives the written tensor:

```wl
Normal[double[out, TTensorCreate[{1., 2., 3., 4.}]]]
```

<!-- => {2., 4., 6., 8.} -->

## Applications

A matrix:

```wl
m = TTensorCreate[{{1., 2.}, {3., 4.}}]
```

<!-- => a TTensor summary box: shape 2 x 2, dtype f32 -->

The kernel of its square:

```wl
k = TKernel[TMaterialize[m . m]]
```

<!-- => a TKernel summary box: output numel 4, output dtype f32 -->

Dispatch it:

```wl
Normal[k[]]
```

<!-- => {{7., 10.}, {15., 22.}} -->

The time the dispatch took, in microseconds, for profiling:

```wl
k["AvgUs"]
```

<!-- => a positive real number, which depends on the machine -->

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

The `"Flops"` property is what [TKernelFlops]() gives for the kernel's id:

```wl
k["Flops"] === TKernelFlops[k["Kid"]]
```

<!-- => True -->

A graph-node constructor takes the kernel object in place of the tensor it writes, here adding *x* to $x^2$:

```wl
Normal[TRealize[TUOpAdd[k, x]]]
```

<!-- => {2., 6., 12., 20., 30., 42., 56., 72.} -->

Arithmetic takes the kernel object in the same way:

```wl
Normal[TRealize[k + x]]
```

<!-- => {2., 6., 12., 20., 30., 42., 56., 72.} -->

## Possible Issues

An id past the newest kernel gives a [Failure]():

```wl
TKernel[TKernelCount[] + 100]
```

<!-- => Failure["TKernel", <|"MessageTemplate" -> "No pinned UOP_KERNEL heap cell found for kid `1`", "MessageParameters" -> {the id}|>] -->

---

A tensor is not a kernel, and [TKernel]() returns unevaluated:

```wl
TKernel[TTensorCreate[{1., 2.}]]
```

<!-- => TKernel[a TTensor summary box] -->
