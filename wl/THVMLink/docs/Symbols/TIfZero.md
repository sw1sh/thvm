---
Template: Symbol
Name: TIfZero
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TIfZero
Keywords: [if, zero, conditional, control-flow, mat]
SeeAlso: [TMatNum, TMatChain, TOp2]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TIfZero]()[*counter*, *thenTerm*, *elseTerm*]</code> -- sugar for a two-arm conditional: *thenTerm* if *counter* reduces to <code>[TNum]()[0]</code>, *elseTerm* otherwise.

## Details & Options

- Expands to `[TApp](){}[[TMatNum]()[0, thenTerm, [TLam](){_, elseTerm}], counter]`. The `else` branch is wrapped in a discarding lambda so the matched `NUM(n>0)` is consumed; the WL-side reads like a plain conditional.
- *counter* is forced to WHNF before the dispatch; a `SUP` on the counter commutes via `APP-MAT-SUP`, so the conditional distributes over a superposed value.
- Useful as a base case for recursive references / `TRef`-driven loops.

## Basic Examples

Zero counter takes the then-branch:

```wl
TReset[];
TTermVal @ TWnf @ TIfZero[TNum[0], TNum[10], TNum[20]]
```
<!-- => 10 -->

Non-zero counter takes the else-branch:

```wl
TReset[];
TTermVal @ TWnf @ TIfZero[TNum[5], TNum[10], TNum[20]]
```
<!-- => 20 -->

## Properties and Relations

For a generic case tree over multiple values, use [TMatChain](); for an integer compare-and-branch use [TOp2]()`["=="]` plus `TIfZero`.
