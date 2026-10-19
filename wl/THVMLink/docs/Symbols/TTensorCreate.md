---
Template: Symbol
Name: TTensorCreate
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TTensorCreate
Keywords: [tensor, create, array, NumericArray, dtype, f32, i32]
SeeAlso: [TTensorData, TTensorShape, TTensorDType, TRealize, TSet, NumericArray]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TTensorCreate]()[*data*]</code> gives a tensor holding the numbers in *data*, with its shape and dtype taken from *data*.

<code>[TTensorCreate]()[*data*, "*dtype*"]</code> gives a tensor of the named *dtype*.

<!-- #| annotation: 26.10.19: TTensorCreate lifts its data to a NumericArray (a packed array or nested list is converted once) and wraps it as a TEN on the active backend; on the CPU a NumericArray of the target type is shared zero-copy. The typed path packs f16, bf16, fp8 and int4 data itself. A scalar, a ragged or empty list or a string reaches the library function unchecked and gives LibraryFunction::lfie and a malformed TTerm[LibraryFunction[...][...]]; an unknown dtype name such as "bogus" gives a tensor with no message; real data with dtype "i32" gives NumericArray::nconvsa and a malformed TTerm (all reported 26.10.19). -->

## Details & Options

- *data* is a rectangular list of numbers of any depth, a packed array, or a [NumericArray]().
- Without a *dtype*, a list of reals gives an `"f32"` tensor, a list of integers an `"i32"` tensor, and a list mixing the two an `"f32"` tensor. A [NumericArray]() keeps its type: `"Real32"` gives `"f32"`, `"Real64"` gives `"f64"`, `"Integer32"` gives `"i32"` and `"UnsignedInteger8"` gives `"u8"`.
- The named *dtype* may be `"f32"`, `"f64"`, `"f16"`, `"bf16"`, `"i32"`, `"i64"` or `"u8"`.
- The result is a tensor that holds its values, displayed as a summary box with its shape and dtype. <code>[Normal]()</code> reads the values back, and arithmetic on it builds lazy graphs for <code>[TRealize]()</code>.
- <code>[TGrad]()</code> differentiates every floating-point tensor a loss depends on, so a tensor from [TTensorCreate]() can be trained with no further marking.
- Each application gives a new tensor, also for equal data. <code>[TSet]()</code> changes the values of an existing tensor.

## Basic Examples

A 2 x 3 tensor of reals:

```wl
x = TTensorCreate[{{1., 2., 3.}, {4., 5., 6.}}]
```

<!-- => a TTensor summary box: shape 2 x 3, dtype f32 -->

Its values:

```wl
Normal[x]
```

<!-- => {{1., 2., 3.}, {4., 5., 6.}} -->

Its shape:

```wl
TTensorShape[x]
```

<!-- => {2, 3} -->

Its dtype:

```wl
TTensorDType[x]
```

<!-- => "f32" -->

## Scope

A list of integers gives an `"i32"` tensor:

```wl
TTensorDType[TTensorCreate[{1, 2, 3}]]
```

<!-- => "i32" -->

A list mixing integers and reals gives an `"f32"` tensor:

```wl
TTensorDType[TTensorCreate[{1, 2.5}]]
```

<!-- => "f32" -->

A [NumericArray]() keeps its type, here 64-bit reals:

```wl
TTensorDType[TTensorCreate[NumericArray[{1., 2.}, "Real64"]]]
```

<!-- => "f64" -->

A tensor of rank 3:

```wl
TTensorCreate[ArrayReshape[Range[1., 24.], {2, 3, 4}]]
```

<!-- => a TTensor summary box: shape 2 x 3 x 4, dtype f32 -->

---

Integers stored as reals:

```wl
TTensorCreate[{1, 2}, "f32"]
```

<!-- => a TTensor summary box: shape 2, dtype f32 -->

Half-precision reals:

```wl
TTensorCreate[{0.5, -0.25, 1.}, "f16"]
```

<!-- => a TTensor summary box: shape 3, dtype f16 -->

Brain floating-point reals, read back as ordinary reals:

```wl
Normal[TTensorCreate[{0.5, -0.25, 1.}, "bf16"]]
```

<!-- => {0.5, -0.25, 1.} -->

64-bit integers:

```wl
TTensorCreate[{1, 2}, "i64"]
```

<!-- => a TTensor summary box: shape 2, dtype i64 -->

Bytes:

```wl
TTensorCreate[{0, 128, 255}, "u8"]
```

<!-- => a TTensor summary box: shape 3, dtype u8 -->

## Applications

The squares of the integers from 1 to 10, as a tensor of reals:

```wl
t = TTensorCreate[Range[10]^2, "f32"]
```

<!-- => a TTensor summary box: shape 10, dtype f32 -->

Their sum, computed by the runtime:

```wl
Normal[TRealize[Total[t]]]
```

<!-- => {385.} -->

## Properties and Relations

A [NumericArray]():

```wl
na = NumericArray[{1., 2., 3.}, "Real32"]
```

<!-- => NumericArray[{1., 2., 3.}, "Real32"] -->

[TTensorData]() reads the same array back:

```wl
TTensorData[TTensorCreate[na]] === na
```

<!-- => True -->

---

Each application gives a new tensor, even for equal data:

```wl
TTensorCreate[{1., 2.}] === TTensorCreate[{1., 2.}]
```

<!-- => False -->

## Possible Issues

An `"f32"` tensor holds about seven significant digits, so 0.1 is stored as the nearest single-precision number:

```wl
First[Normal[TTensorCreate[{0.1}]]] - 0.1
```

<!-- => 1.49012*10^-9 -->

Half precision holds about three digits:

```wl
Normal[TTensorCreate[{0.1}, "f16"]]
```

<!-- => {0.0999756} -->

---

A list of machine reals gives an `"f32"` tensor, not `"f64"`, so its values are rounded to single precision:

```wl
TTensorDType[TTensorCreate[{1., 2.}]]
```

<!-- => "f32" -->
