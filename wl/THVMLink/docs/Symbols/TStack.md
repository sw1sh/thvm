---
Template: Symbol
Name: TStack
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TStack
Keywords: [stack, eliminator, frames, wnf, debug]
SeeAlso: [TStep, TWnf, TItrs]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TStack]()[]</code> returns the eliminator frames pending at the most recent bail point of [TStep]() / <code>[TWnf]()[_, *n*]</code>.

## Details & Options

- Each frame is a `TTerm` tagged `APP`, `DP0`, or `DP1`; the `VAL` field points at the heap loc of the original cell so the eliminator can be re-entered.
- Returns `{}` for unbounded [TWnf]() (which always drains its stack) or when a bounded run completed within budget.
- Used by step-by-step inspectors and the multicomputation trace ([TMultiTrace]()) to know what is still pending after one fire.

## Basic Examples

A completed run leaves no pending frames:

```wl
TReset[];
TWnf[TApp[TLam[x, TOp2["+", x, TNum[1]]], TNum[41]]];
TStack[]
```
<!-- => {} -->

## Properties and Relations

The bail mechanism is described in `src/wnf/_.c`: on `BUDGET_HIT`, the in-flight WHNF is written back through each frame's slot before the C side returns, so the heap is in a resumable state. Use [TItrs]() for the interaction counter.
