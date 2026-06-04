---
Template: Symbol
Name: TTensorData
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TTensorData
Keywords: [tensor, buffer, NumericArray, read back, dtype]
SeeAlso: [TTensorShape, TTensorDType, TTensorCreate, TRealize, TSet]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TTensorData]()[*t*]</code> reads tensor *t*'s backing buffer as a [NumericArray]() whose type matches the dtype (`"Real32"` for f32, `"Integer32"` for i32).

## Details & Options

- Wrap the result in <code>[Normal]()</code> to get a plain nested list of numbers; the [Normal]() UpValue on a realized tensor means exactly `Normal[TTensorData[...]]`.
- The argument must be a realized tensor; call <code>[TRealize]()</code> on a UOp graph first, since lazy nodes carry no buffer.
- The element type mirrors <code>[TTensorDType]()</code>: f32 tensors read back as a `"Real32"` [NumericArray](), i32 as `"Integer32"`.

## Basic Examples

Read back a freshly created tensor:

```wl
TTensorData @ TTensorCreate[{1., 2., 3., 4.}]
```
<!-- => NumericArray[{1., 2., 3., 4.}, "Real32"] -->

## Scope

An i32 tensor reads back as an `"Integer32"` [NumericArray]():

```wl
TTensorData @ TTensorCreate[{1, 2, 3}, "i32"]
```
<!-- => NumericArray[{1, 2, 3}, "Integer32"] -->

---

<code>[Normal]()</code> projects a realized buffer to a plain list:

```wl
x = TTensorCreate[{1., 2., 3., 4.}];
Normal @ TRealize @ (x*x)
```
<!-- => {1., 4., 9., 16.} -->
