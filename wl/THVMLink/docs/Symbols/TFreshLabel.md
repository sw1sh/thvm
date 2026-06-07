---
Template: Symbol
Name: TFreshLabel
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TFreshLabel
Keywords: [label, sup, dup, fresh, counter]
SeeAlso: [TSup, TDup, TReset]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TFreshLabel]()[]</code> returns the next integer from a monotonic SUP/DUP label counter and bumps it. Reset by [TReset]().

## Details & Options

- `TSup` and `TDup` consume a fresh label when called without an explicit one. The user-facing counter starts at 1 and counts up.
- The auto-DUP inserted by `TLam` for multi-use binders uses a separate label range (`0x10000..0x1FFFF`) so it cannot collide with these labels.
- The counter is per-runtime: [TReset]() zeroes it (the labels start over at 1); [TInit]() does not.

## Basic Examples

Two successive calls return consecutive integers:

```wl
TReset[];
TFreshLabel[]
```
<!-- => 1 -->

```wl
TFreshLabel[]
```
<!-- => 2 -->

## Properties and Relations

Use `TFreshLabel` when constructing a `SUP`/`DUP` pair that must share a label (so the matching `DUP-SUP-ANN` fires) but the label itself must not collide with anything already in the heap. The label-collision semantics behind the `ANN` vs `COM` choice are detailed in `docs/multicomputation.md`.
