---
Template: Symbol
Name: TTensorDType
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TTensorDType
Keywords: [tensor, dtype, element type, f32, i32, precision]
SeeAlso: [TTensorCreate, TTensorShape, TTensorData, TRealize, NumericArrayType]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TTensorDType]()[*t*]</code> gives the dtype of the tensor *t* as a string, such as `"f32"` or `"i32"`.

<!-- #| annotation: 26.10.19: TTensorDType names the dtype code in the TEN term's ext field. A number combined with a tensor is lifted to the tensor's dtype (liftNumeric), except that a Real or Rational with an integer tensor gives f32 (scalarDType), and reciprocal and square root cast an integer tensor to f32 first (ensureFloat), as tinygrad promotes; so n/2 and n/m of i32 tensors are true division. -->

## Details & Options

- The dtypes are `"f32"`, `"f64"`, `"f16"` and `"bf16"` for floating-point tensors, and `"i32"`, `"i64"` and `"u8"` for integer tensors, among others.
- <code>[TTensorCreate]()</code> gives `"f32"` for a list of reals and `"i32"` for a list of integers. <code>[TGlorot]()</code> gives `"f32"`.
- The result of a computation has the dtype of its tensor operands: a comparison of `"f32"` tensors is an `"f32"` mask of 0 and 1, and the sum of an `"i32"` tensor is `"i32"`. Combining an `"i32"` and an `"f32"` tensor gives `"f32"`.
- A number combined with a tensor takes the tensor's dtype, except that a real number combined with an integer tensor gives `"f32"`, as in tinygrad: an `"i32"` tensor times 2.5 is `"f32"`, and times 3 it stays `"i32"`.
- Only a tensor that holds its values has a dtype here. For a lazy graph, <code>[TTensorDType]()</code> gives <code>[Missing]()["NotATensor", "UOP"]</code>.
- An argument that is not a [TTerm]() returns unevaluated.

## Basic Examples

A tensor of reals is single precision:

```wl
TTensorDType[TTensorCreate[{1., 2., 3.}]]
```

<!-- => "f32" -->

## Scope

A tensor of integers is `"i32"`:

```wl
TTensorDType[TTensorCreate[{1, 2, 3}]]
```

<!-- => "i32" -->

A named dtype:

```wl
TTensorDType[TTensorCreate[{1., 2.}, "f16"]]
```

<!-- => "f16" -->

A [TGlorot]() weight:

```wl
TTensorDType[TGlorot[{2, 4}]]
```

<!-- => "f32" -->

---

An integer tensor:

```wl
n = TTensorCreate[{1, 2, 3}]
```

<!-- => a TTensor summary box: shape 3, dtype i32 -->

Its sum is an integer:

```wl
TTensorDType[TRealize[Total[n]]]
```

<!-- => "i32" -->

Adding a real tensor gives reals:

```wl
TTensorDType[TRealize[n + TTensorCreate[{0.5, 0.5, 0.5}]]]
```

<!-- => "f32" -->

Multiplying by a real number gives reals too:

```wl
TTensorDType[TRealize[n*2.5]]
```

<!-- => "f32" -->

The product keeps the fractions:

```wl
Normal[TRealize[n*2.5]]
```

<!-- => {2.5, 5., 7.5} -->

Multiplying by an integer keeps the integers:

```wl
TTensorDType[TRealize[n*3]]
```

<!-- => "i32" -->

## Applications

A half-precision tensor:

```wl
h = TTensorCreate[{1., 2.}, "f16"]
```

<!-- => a TTensor summary box: shape 2, dtype f16 -->

Arithmetic keeps the half precision through the computation:

```wl
TTensorDType[TRealize[h*2]]
```

<!-- => "f16" -->

## Properties and Relations

A tensor of reals:

```wl
x = TTensorCreate[{1., 2., 3.}]
```

<!-- => a TTensor summary box: shape 3, dtype f32 -->

The dtype determines the type of the [NumericArray]() that [TTensorData]() gives:

```wl
NumericArrayType[TTensorData[x]]
```

<!-- => "Real32" -->

A comparison of real tensors is a real mask of 0 and 1:

```wl
TTensorDType[TRealize[x < 2]]
```

<!-- => "f32" -->

## Possible Issues

A lazy graph has no dtype here; realize it first:

```wl
TTensorDType[TTensorCreate[{1., 2.}]^2]
```

<!-- => Missing["NotATensor", "UOP"] -->
