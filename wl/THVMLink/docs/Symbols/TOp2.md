---
Template: Symbol
Name: TOp2
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TOp2
Keywords: [op2, arithmetic, integer, num, operator]
SeeAlso: [TNum, TEql, TIfZero, TMatNum]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TOp2]()[*opcode*, *x*, *y*]</code> constructs a `TAG_OP2` integer-arithmetic node. *opcode* is one of `"+"`, `"-"`, `"*"`, `"=="`, `"<"`. Bare-Integer operands are coerced to `TAG_NUM`.

## Details & Options

- Strict on both operands: the `OP2-NUM-NUM` rule fires only once both have reduced to `TAG_NUM`. Until then the `OP2` sits in the heap waiting for its left, then its right, to reach WHNF.
- `OP2-SUP` (left or right) slides the operator into each branch of a `SUP`, so <code>[TSup]()[1, 2] + 3</code> reduces to a `SUP` whose arms are <code>[TOp2]()["+", ...]</code> on the branches.
- For the WL-side numeric UpValues, `[Plus](){}{[TNum]()[1], [TNum]()[2]}` etc. route through `TOp2` directly so ordinary <code>+ - *</code> on `TTerm`s build OP2 trees -- see `numericTermQ` in `Switch.wl`.
- For *structural* equality over arbitrary terms (SUPs, CTRs, LAMs) prefer [TEql](); `TOp2["=="]` only compares the integer `VAL` field.

## Basic Examples

Pure-integer fold:

```wl
TReset[];
TTermVal @ TWnf @ TOp2["+", TNum[3], TNum[4]]
```
<!-- => 7 -->

## Scope

`OP2-SUP` slides arithmetic into a superposition:

```wl
TReset[];
TTermVal /@ TCollapse[TOp2["+", TSup[10, 20], TNum[1]]]
```
<!-- => {11, 21} -->

## Properties and Relations

The `==` and `<` opcodes return `TNum[1]` / `TNum[0]`; gate them with [TIfZero]() or [TMatNum]() to build conditional graphs.
