---
Template: Symbol
Name: TUOpConst
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TUOpConst
Keywords: [UOp, const, scalar, literal, dtype]
SeeAlso: [TUOpAdd, TUOpMul, TUOpNeg, TTensorCreate, TRealize]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TUOpConst]()[*value*, *dtype*]</code> builds a `UOP_CONST` node carrying the scalar *value* with *dtype* either `"f32"` or `"i32"`.

## Details & Options

- Use it to introduce a literal into a UOp graph; bare integers and reals are not lifted automatically by <code>[TUOpAdd]()</code> or <code>[TUOpMul]()</code>.
- The node is a scalar; it broadcasts against tensor operands in elementwise ops.
- Lazy: realizing it yields a length-1 buffer of the chosen dtype.

## Basic Examples

A scalar f32 constant realizes to a length-1 buffer:

```wl
Normal @ TRealize @ TUOpConst[5., "f32"]
```
<!-- => {5.} -->

## Scope

The `"i32"` dtype produces an integer constant:

```wl
Normal @ TRealize @ TUOpConst[7, "i32"]
```
<!-- => {7} -->

## Properties and Relations

A constant broadcasts against a tensor in an elementwise multiply:

```wl
x = TTensorCreate[{1., 2., 3., 4.}];
Normal @ TRealize @ TUOpMul[x, TUOpConst[10., "f32"]]
```
<!-- => {10., 20., 30., 40.} -->
