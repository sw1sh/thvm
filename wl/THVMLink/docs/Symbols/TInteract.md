---
Template: Symbol
Name: TInteract
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TInteract
Keywords: [interact, redex, fire, step, debug]
SeeAlso: [TRedexes, TStep, TItrs, TMultiTrace]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TInteract]()[*redex*]</code> fires exactly one interaction at *redex* -- a specific `TTerm` redex, not the active head.

## Details & Options

- Returns an `Association` with keys `"result"` (the post-fire `TTerm` -- what replaces the redex) and `"fresh"` (a `List` of `TTerm`s whose redex status flipped because of this fire, locally produced or propagated through shared subgraphs).
- Returns <code>[Failure]()["NotARedex", ...]</code> if *redex* is no longer reducible -- for example, a prior fire already consumed the cell.
- Use this for non-head-driven exploration: pair with [TRedexes]() to enumerate every redex on the live heap, then pick one to fire.

## Basic Examples

Fire the head of an APP-LAM redex:

```wl
TReset[];
seed = TApp[TLam[x, x], TNum[5]];
res  = TInteract[First @ TRedexes[seed]];
res["result"] // TTermVal
```
<!-- => 5 -->

## Properties and Relations

[TStep]() drives one interaction at the *head* via `TWnf[_, 1]`; `TInteract` is the *targeted* one-shot fire used by interactive multiway explorers and by [TMultiTrace]() under the hood.
