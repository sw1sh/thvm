---
Template: Symbol
Name: TContextStrip
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TContextStrip
Keywords: [context, snapshot, strip, tensors, shape-only]
SeeAlso: [TContext, TContextSnapshot, TInitialize]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TContextStrip]()[*h*]</code> returns *h* with every `NumericArray` tensor buffer replaced by <code><|"shape" -> _, "dtype" -> _|></code>. Pure WL; does not touch the runtime.

## Details & Options

- Useful when the snapshot is destined for shape-checking, diffing, or wire transmission and the tensor data would be wasteful payload.
- Round-trips through [TInitialize]() with `"ZeroFill" -> True` so tensors are allocated zero-filled on restore.

## Properties and Relations

[TContextSnapshot]() captures the live runtime; `TContextStrip` removes the buffers; [TInitialize]() restores it.
