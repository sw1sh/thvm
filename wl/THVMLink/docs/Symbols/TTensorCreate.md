---
Template: Symbol
Name: TTensorCreate
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TTensorCreate
Keywords: [tensor, numeric array, dtype, zero copy]
SeeAlso: [TTensor, TTensorData, TTensorShape, TTensorDType, TSet]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TTensorCreate]()[$data$]</code> wraps $data$ in a `TTerm` holding a fresh `TAG_TEN` whose shape and dtype are inferred from the input.

<code>[TTensorCreate]()[$data$, $dtype$]</code> coerces $data$ to the named $dtype$ ("f32", "i32", "f16", ...) before allocation.

## Details & Options

- Accepts a `NumericArray`, a `PackedArray`, or a nested `List` of numbers. The first two are shared zero-copy on CPU; nested lists are first lifted to a `NumericArray` (one copy) and then shared.
- Backend selection is global: tensors are allocated on the active context's default device (see <code>[TContext]()</code> and the `DEV` environment variable). Per-tensor backend selection is a follow-up.
- Returns the same `TTerm[id]` shape the rest of the API consumes - feed it into any `TUOp*` constructor, differentiate through it with <code>[TGrad]()</code> (every float leaf is auto-graded), or read it back with <code>[TTensorData]()</code>.
- The `dtype` defaults to "f32" when the input is a Real list; integer inputs default to "i32".

## Basic Examples

Wrap a nested list as a 2x3 float tensor:

```wl
Needs["WolframInstitute`THVMLink`"];
TInit[];
x = TTensorCreate[{{1., 2., 3.}, {4., 5., 6.}}];
{TTensorShape[x], TTensorDType[x]}
```
<!-- => {{2, 3}, "f32"} -->

## Scope

Round-trip a `NumericArray` through the runtime (zero copy on CPU):

```wl
na = NumericArray[Range[0., 7.], "Real32"];
t  = TTensorCreate[na];
TTensorData[t]
```
<!-- => NumericArray[{0., 1., 2., 3., 4., 5., 6., 7.}, "Real32"] -->

---

Coerce a Real list into half precision:

```wl
TTensorDType @ TTensorCreate[{0.5, -0.25, 1.0}, "f16"]
```
<!-- => "f16" -->

## Applications

Bring a Wolfram-side dataset into a training graph:

```wl
weights = TTensorCreate[RandomReal[{-0.1, 0.1}, {10, 4}]];
inputs  = TTensorCreate[ConstantArray[1., {4}]];
TTensorShape @ TRealize @ TMatVec[weights, inputs]
```
<!-- => {10} -->

## Properties and Relations

`TTensorCreate[a, d]` is equivalent to writing the data into a freshly allocated <code>[TTensor]()</code>:

```wl
SameQ[
    TTensorData @ TTensorCreate[{1., 2., 3.}, "f32"],
    TTensorData @ TTensor[{3}, {1., 2., 3.}]
]
```
<!-- => True -->

## Possible Issues

The runtime selects backend by global context. `TTensorCreate` does not yet take a `Backend` option; switch contexts first if you need a Metal tensor:

```wl
metal = TContextNew["metal"];
TInContext[ metal,
    TTensorCreate[Range[0., 3.]]
]
```
<!-- => a TTerm whose Metal buf-id is non-zero -->

## Neat Examples

The tensor's heap representation surfaces directly in <code>[THeapGraph]()</code>:

```wl
THeapGraph @ TTensorCreate[{{1., 2.}, {3., 4.}}]
```
<!-- => a single TEN node, since the tensor is heap-resident with no inbound combinator yet -->
