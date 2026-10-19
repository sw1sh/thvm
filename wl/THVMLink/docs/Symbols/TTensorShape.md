---
Template: Symbol
Name: TTensorShape
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TTensorShape
Keywords: [tensor, shape, dimensions, rank]
SeeAlso: [TTensorCreate, TTensorDType, TTensorData, TRealize, Dimensions]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TTensorShape]()[*t*]</code> gives the shape of the tensor *t* as a list of integers.

<!-- #| annotation: 26.10.19: TTensorShape reads the shape of the TenDesc behind a TEN term. The Dimensions UpValue on TTerm walks the static shape of a UOp graph instead (tUopShape), which is why it also answers for lazy graphs. -->

## Details & Options

- The length of the shape is the rank of the tensor, and a scalar has shape `{}`.
- Only a tensor that holds its values has a shape here. For a lazy graph, <code>[TTensorShape]()</code> gives <code>[Missing]()["NotATensor", "UOP"]</code>.
- <code>[Dimensions]()</code> gives the shape of a tensor and also of a lazy graph, without computing it.
- An argument that is not a [TTerm]() returns unevaluated.

## Basic Examples

The shape of a vector:

```wl
TTensorShape[TTensorCreate[{1., 2., 3., 4.}]]
```

<!-- => {4} -->

## Scope

The shape of a tensor of rank 4:

```wl
TTensorShape[TTensorCreate[ConstantArray[0., {2, 3, 4, 5}]]]
```

<!-- => {2, 3, 4, 5} -->

---

A vector of six elements:

```wl
x = TTensorCreate[Range[1., 6.]]
```

<!-- => a TTensor summary box: shape 6, dtype f32 -->

A reshape changes the shape:

```wl
TTensorShape[TRealize[ArrayReshape[x, {2, 3}]]]
```

<!-- => {2, 3} -->

A transpose reverses it:

```wl
TTensorShape[TRealize[Transpose[ArrayReshape[x, {2, 3}]]]]
```

<!-- => {3, 2} -->

A sum is a scalar:

```wl
TTensorShape[TRealize[Total[x]]]
```

<!-- => {} -->

## Applications

The shape of a matrix product:

```wl
TTensorShape[TRealize[TTensorCreate[ConstantArray[1., {2, 3}]] . TTensorCreate[ConstantArray[1., {3, 5}]]]]
```

<!-- => {2, 5} -->

## Properties and Relations

A vector of six elements:

```wl
x = TTensorCreate[Range[1., 6.]]
```

<!-- => a TTensor summary box: shape 6, dtype f32 -->

[Dimensions]() gives the shape of a lazy graph without computing it:

```wl
Dimensions[Transpose[ArrayReshape[x, {2, 3}]]]
```

<!-- => {3, 2} -->

The shape is also the [Dimensions]() of the values:

```wl
TTensorShape[x] === Dimensions[Normal[x]]
```

<!-- => True -->

## Possible Issues

A vector:

```wl
x = TTensorCreate[{1., 2., 3.}]
```

<!-- => a TTensor summary box: shape 3, dtype f32 -->

A lazy graph has no shape here; realize it first, or use [Dimensions]():

```wl
TTensorShape[x^2]
```

<!-- => Missing["NotATensor", "UOP"] -->

---

A scalar has shape `{}`, while its values read back as a list of one element:

```wl
Normal[TRealize[Total[TTensorCreate[{1., 2., 3.}]]]]
```

<!-- => {6.} -->
