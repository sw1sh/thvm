---
Template: Symbol
Name: TKernelFlops
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TKernelFlops
Keywords: [kernel, flops, estimate, cost, profile]
SeeAlso: [TKernel, TKernelCount, TKernelSource, TKernelProposed, TRealize]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TKernelFlops]()[*kid*]</code> returns a static FLOPS estimate for one execution of kernel *kid*.

## Details & Options

- The estimate sums over the lifted UOp DAG: one flop per elementwise op per element, plus one flop per reduce-source element.
- Movement and load ops contribute 0.
- Equivalent to the <code>[TKernel]()[*kid*]["Flops"]</code> property; obtain a `kid` from <code>[TKernelCount]()[] - 1</code> after a realize (see <code>[TKernelCount]()</code>).

## Basic Examples

A kernel that squares an 8-element input and sums it does one multiply per element plus one flop per reduce-source element, so 16 flops:

```wl
x = TTensorCreate[Range[1., 8.]];
TRealize[Total[x^2]];
kid = TKernelCount[] - 1;
TKernelFlops[kid]
```
<!-- => 16 -->

## Properties and Relations

<code>[TKernelFlops]()[*kid*]</code> agrees with the kernel object's `"Flops"` property:

```wl
x = TTensorCreate[Range[1., 8.]];
TRealize[Total[x^2]];
kid = TKernelCount[] - 1;
TKernelFlops[kid] === TKernel[kid]["Flops"]
```
<!-- => True -->
