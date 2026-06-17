---
Template: Symbol
Name: TEql
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TEql
Keywords: [equality, eql, structural, theorem-proving]
SeeAlso: [TOp2, TFindProof, TSatEUF, TTermEq]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TEql]()[*a*, *b*]</code> returns a `TAG_EQL` term that reduces to <code>[TNum]()[1]</code> iff *a* and *b* are structurally equal, <code>[TNum]()[0]</code> otherwise.

## Details & Options

- Strict on both arguments: both sides are driven to WHNF before the rule fires.
- A `SUP` on either side commutes (clones the other side via [TDup]() and distributes equality through the branches); `ERA` and `ANY` short-circuit.
- `NUM-NUM` compares the integer `VAL` field. `CTR-CTR` and `LAM-LAM` are deeper rules landing with the HVM4 port -- currently they fall through to a stuck rebuild rather than reducing.
- Prefer `TEql` over `[TOp2]()["==", ...]` when the operands may be superpositions, constructors, or lambdas: `TOp2["=="]` is the integer-only path.
- For a host-side (WL) comparison driven through `cnf`, see `TTermEq` / `TTermSame`.

## Basic Examples

Two equal numbers:

```wl
TReset[];
TTermVal @ TWnf @ TEql[TNum[1], TNum[1]]
```
<!-- => 1 -->

Two distinct numbers:

```wl
TReset[];
TTermVal @ TWnf @ TEql[TNum[1], TNum[2]]
```
<!-- => 0 -->

## Properties and Relations

The theorem-proving stack ([TFindProof](), [TSatEUF]()) uses `TEql` to represent goal-direction equality between heap terms; the structural rules let it survive `SUP` distribution without collapsing the search space.
