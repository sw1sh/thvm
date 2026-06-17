---
Template: Symbol
Name: TNum
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TNum
Keywords: [number, num, integer, atom]
SeeAlso: [TOp2, TUOpConst, TIfZero, TCtr]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TNum]()[*i*]</code> returns a `TAG_NUM` atom holding the integer *i* (`DT_I32`).

<code>[TNum]()[*i*, *dtype*]</code> picks the dtype: `"i32"` (default) or `"f32"` -- for `"f32"` the value is bit-reinterpreted from the integer bits.

## Details & Options

- `TAG_NUM` is the IC's integer atom. Many of the higher-level constructors ([TSup](), [TApp](), [TCtr](), ...) auto-coerce a bare `Integer` argument to <code>[TNum]()[*i*]</code>, so you rarely need to write it explicitly.
- For arithmetic floats use [TUOpConst]() instead; the `f32` form of `TNum` is for bit-pattern packing (e.g. embedded scalar payloads).
- Atomic: `TNum` is its own WNF. Arithmetic over `TNum`s fires via [TOp2]().

## Basic Examples

```wl
TReset[];
Term[TNum[42]]
```
<!-- => Term["NUM", 42] -->

The integer value is in the `VAL` field:

```wl
TReset[];
TTermVal[TNum[42]]
```
<!-- => 42 -->

## Scope

Arithmetic over `TNum`s fires `OP2-NUM-NUM`:

```wl
TReset[];
TTermVal @ TWnf @ TOp2["+", TNum[3], TNum[4]]
```
<!-- => 7 -->

## Properties and Relations

A bare `Integer` in a constructor slot is coerced to `TNum` via `numCoerce`; the `+`, `*`, `-`, `==`, `<` UpValues route through [TOp2](). For a tensor-valued constant use [TUOpConst]().
