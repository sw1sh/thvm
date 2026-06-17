---
Template: Symbol
Name: TAtpSchedule
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TAtpSchedule
Keywords: [ATP, Method, portfolio, schedule, introspection]
SeeAlso: [TAtpDescribeMethod, TFindProof, TRelevantAxioms]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TAtpSchedule]()[$method$]</code> returns the schedule (a list of single-config `Method` specs) that <code>[TFindProof]()[..., Method -> $method$]</code> would expand to, without running the C engine.  Useful for debugging a `Method` choice or counting portfolio entries before allocating `TimeConstraint`.

<code>[TAtpSchedule]()[$method$, $conjecture$, $axioms$]</code> threads the conjecture and axioms through `Automatic`'s structure-recognised auto-tune, so the returned schedule matches what <code>[TFindProof]()[$conjecture$, $axioms$, Method -> $method$]</code> would dispatch.

<code>[TAtpSchedule]()[$method$, "$Theorem$", "$Theory$"]</code> resolves names through [AxiomaticTheory]().

## Details & Options

- A single explicit config like `{"Completion", "Ordering" -> "LPO"}` returns a one-entry list `{<that config>}`.
- A named preset like `"Waldmeister"` returns the same one-entry list with the preset name.
- A portfolio like `"VampirePortfolio"` returns the multi-entry rotation.
- `Automatic` (the default) returns the structure-aware front + the fixed `"Portfolio"` tail.  Without the conjecture / axioms it returns the un-front-loaded `"Portfolio"`.

## Basic Examples

A named portfolio expands to its rotation:

```wl
TAtpSchedule["VampirePortfolio"]
```
<!-- => {{"VampireUEQ"}, {"Completion", "CriticalPairWeight" -> "Twee", ...}, ...} -->

An `Automatic` schedule against an AbelianGroup goal carries the structure-aware front:

```wl
TAtpSchedule[Automatic,
    Inactive[Equal][x*y, y*x], "AbelianGroupAxioms"]
```
<!-- => structure-aware front + the fixed "Portfolio" tail -->

A single-config `Method` round-trips unchanged:

```wl
TAtpSchedule[{"Completion", "Ordering" -> "LPO"}]
```
<!-- => {{"Completion", "Ordering" -> "LPO"}} -->

## Properties & Relations

- [TAtpDescribeMethod]() is the companion: given the same `Method` spec, it returns the merged options Association each individual entry resolves to.  Pair the two to inspect both shape and content of a portfolio.
- [TFindProof]() with `Method -> $method$` runs the schedule [TAtpSchedule]() returns.  `TimeConstraint` divides fairly across entries; use `PortfolioFrontLoad -> n` to widen the share of the first `n` entries.
