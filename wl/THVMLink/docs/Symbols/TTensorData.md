---
Template: Symbol
Name: TTensorData
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TTensorData
Keywords: [tensor, buffer, NumericArray, read back, values, dtype]
SeeAlso: [TTensorCreate, TTensorShape, TTensorDType, TRealize, Normal, NumericArray]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TTensorData]()[*t*]</code> gives the values of the tensor *t* as a [NumericArray]().

<!-- #| annotation: 26.10.19: TTensorData reads the TEN's buffer as a NumericArray of the matching type (on the CPU it shares the buffer). The Normal UpValue on a TTerm is Normal[TTensorData[t]], except that it decodes the 16-bit floating-point dtypes, whose raw buffer TTensorData returns as UnsignedInteger16 bit patterns. -->

## Details & Options

- The type of the [NumericArray]() follows the dtype of the tensor: `"f32"` gives `"Real32"`, `"f64"` gives `"Real64"`, `"i32"` gives `"Integer32"`, `"i64"` gives `"Integer64"` and `"u8"` gives `"UnsignedInteger8"`.
- For `"f16"` and `"bf16"` tensors, the [NumericArray]() holds the raw 16-bit patterns as `"UnsignedInteger16"`. <code>[Normal]()</code> of the tensor decodes them to reals.
- <code>[Normal]()[*t*]</code> gives the same values as a list, the same as <code>[Normal]()[[TTensorData]()[*t*]]</code> for the other dtypes.
- Only a tensor that holds its values can be read. For a lazy graph, <code>[TTensorData]()</code> gives <code>[Missing]()["NotATensor", "UOP"]</code>; <code>[TRealize]()</code> computes the values first.
- An argument that is not a [TTerm]() returns unevaluated.

## Basic Examples

A tensor:

```wl
x = TTensorCreate[{1., 2., 3., 4.}]
```

<!-- => a TTensor summary box: shape 4, dtype f32 -->

Its values as a [NumericArray]():

```wl
TTensorData[x]
```

<!-- => NumericArray[{1., 2., 3., 4.}, "Real32"] -->

## Scope

An integer tensor reads back as `"Integer32"`:

```wl
TTensorData[TTensorCreate[{1, 2, 3}]]
```

<!-- => NumericArray[{1, 2, 3}, "Integer32"] -->

A 64-bit real tensor reads back as `"Real64"`:

```wl
TTensorData[TTensorCreate[{0.5, 1.}, "f64"]]
```

<!-- => NumericArray[{0.5, 1.}, "Real64"] -->

A byte tensor reads back as `"UnsignedInteger8"`:

```wl
TTensorData[TTensorCreate[{1, 2}, "u8"]]
```

<!-- => NumericArray[{1, 2}, "UnsignedInteger8"] -->

A matrix keeps its shape:

```wl
TTensorData[TTensorCreate[{{1., 2.}, {3., 4.}}]]
```

<!-- => NumericArray[{{1., 2.}, {3., 4.}}, "Real32"] -->

---

A tensor:

```wl
x = TTensorCreate[{1., 2., 3.}]
```

<!-- => a TTensor summary box: shape 3, dtype f32 -->

The result of a computation, once realized:

```wl
TTensorData[TRealize[Total[x]]]
```

<!-- => NumericArray[{6.}, "Real32"] -->

## Applications

A tensor:

```wl
x = TTensorCreate[{1., 2., 3.}]
```

<!-- => a TTensor summary box: shape 3, dtype f32 -->

The array works directly with functions that take a [NumericArray](), such as [NumericArrayType]():

```wl
NumericArrayType[TTensorData[TRealize[x^2]]]
```

<!-- => "Real32" -->

## Properties and Relations

A matrix:

```wl
m = TTensorCreate[{{1., 2.}, {3., 4.}}]
```

<!-- => a TTensor summary box: shape 2 x 2, dtype f32 -->

[Normal]() of the tensor is [Normal]() of its data:

```wl
Normal[m] === Normal[TTensorData[m]]
```

<!-- => True -->

## Possible Issues

A tensor:

```wl
x = TTensorCreate[{1., 2., 3.}]
```

<!-- => a TTensor summary box: shape 3, dtype f32 -->

A lazy graph has no values to read:

```wl
TTensorData[x^2]
```

<!-- => Missing["NotATensor", "UOP"] -->

---

A half-precision tensor:

```wl
h = TTensorCreate[{0.5, 1.}, "f16"]
```

<!-- => a TTensor summary box: shape 2, dtype f16 -->

Its data holds the raw 16-bit patterns:

```wl
TTensorData[h]
```

<!-- => NumericArray[{14336, 15360}, "UnsignedInteger16"] -->

[Normal]() of the tensor decodes them:

```wl
Normal[h]
```

<!-- => {0.5, 1.} -->
