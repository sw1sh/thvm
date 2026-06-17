---
Template: Symbol
Name: TKernelSource
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TKernelSource
Keywords: [kernel, source, codegen, C, Metal]
SeeAlso: [TKernel, TKernelCount, TKernelFlops, TKernelProposed, TRealize]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TKernelSource]()[*kid*]</code> returns the source the kernel *kid* renders to on the active backend.

<code>[TKernelSource]()[*kid*, *backend*]</code> renders for the named *backend*, `"C"` or `"Metal"`.

## Details & Options

- A `kid` comes from realizing a graph and reading <code>[TKernelCount]()[] - 1</code>; see <code>[TKernelCount]()</code>.
- The default *backend* is the active one (the `DEV` environment variable, `"C"` otherwise).
- Returns an empty string when `kernel_lift_to_uop` declines (more than 30 inputs, no scalar arena, and so on) and the kernel falls back to the per-op interpreter.
- Also available as a property: <code>[TKernel]()[*kid*]["Source"]</code> / <code>[TKernel]()[*kid*]["Source", *backend*]</code>.

## Basic Examples

Render the C source of a freshly realized sum-of-squares kernel; it defines an entry point `void k(`:

```wl
x = TTensorCreate[Range[1., 8.]];
TRealize[Total[x^2]];
kid = TKernelCount[] - 1;
StringContainsQ[TKernelSource[kid, "C"], "void k("]
```
<!-- => True -->

## Scope

The same kernel renders a Metal compute entry point on the `"Metal"` backend:

```wl
x = TTensorCreate[Range[1., 8.]];
TRealize[Total[x^2]];
kid = TKernelCount[] - 1;
StringContainsQ[TKernelSource[kid, "Metal"], "kernel void k("]
```
<!-- => True -->
