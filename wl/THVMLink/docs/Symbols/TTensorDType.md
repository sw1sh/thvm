---
Template: Symbol
Name: TTensorDType
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TTensorDType
Keywords: [tensor, dtype, f32, i32, type]
SeeAlso: [TTensorData, TTensorShape, TTensorCreate, TUOpConst, TGlorot]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TTensorDType]()[*t*]</code> returns tensor *t*'s dtype as a string, `"f32"` or `"i32"`.

## Details & Options

- The dtype determines how <code>[TTensorData]()</code> reads the buffer back: `"f32"` yields a `"Real32"` [NumericArray](), `"i32"` an `"Integer32"` one.
- <code>[TTensorCreate]()</code> infers the dtype from its data or coerces it to a supplied `"f32"` / `"i32"`; <code>[TGlorot]()</code>, <code>[TZeros]()</code>, and <code>[TOnes]()</code> always produce `"f32"`.

## Basic Examples

A real-valued tensor is f32:

```wl
TTensorDType @ TTensorCreate[{1., 2., 3.}]
```
<!-- => f32 -->

## Scope

An integer tensor created with the `"i32"` dtype reports `"i32"`:

```wl
TTensorDType @ TTensorCreate[{1, 2, 3}, "i32"]
```
<!-- => i32 -->

---

A <code>[TGlorot]()</code> weight is always f32:

```wl
TTensorDType @ TRealize @ TGlorot[{2, 4}]
```
<!-- => f32 -->
