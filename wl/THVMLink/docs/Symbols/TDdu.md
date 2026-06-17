---
Template: Symbol
Name: TDdu
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TDdu
Keywords: [dynamic label, ddu, dup, hvm4, lazy label]
SeeAlso: [TDup, TDsu, TFreshLabel]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TDdu]()[*label*, *val*, *body*]</code> constructs a dynamic-label DUP (HVM4 `DDU`). Same shape as [TDsu]() but on the DUP side: once *label* resolves to <code>[TNum]()[*n*]</code>, the `DDU` reduces to `body(X0, X1)` where `X0`, `X1` are projections of <code>[TDup]()[*n*, *val*]</code>.

## Details & Options

- *body* must be a 2-arg lambda pair: the runtime supplies the two projections positionally.
- The *label* is forced to WHNF (strict-left); only `TAG_NUM` labels trigger the rewrite to a static [TDup]().
- Useful for parser / matcher constructions that need a per-occurrence DUP label and would otherwise have to fabricate one outside the term graph.

## Properties and Relations

`TDdu` is the dual of [TDsu](); both lift the SUP/DUP label into the term layer rather than committing it at construction time, mirroring HVM4's `DSU` and `DDU` primitives.
