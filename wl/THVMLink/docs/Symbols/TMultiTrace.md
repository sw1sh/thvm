---
Template: Symbol
Name: TMultiTrace
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TMultiTrace
Keywords: [multicomputation, trace, steps, events, slice, hvm4]
SeeAlso: [TMultiTraceQ, TMultiwayGraph, TCausalGraph, TCollapse, TStep]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TMultiTrace]()[*expr*]</code> evaluates *expr* (HoldFirst) with the multicomputation reduction trace recording on, then runs the collapse walk one [TStep]() at a time, returning a `List` of per-step `Association`s.

<code>[TMultiTrace]()[*expr*, *keys*]</code> projects each step record to the requested keys. Available keys: `"Step"`, `"Term"`, `"Events"`, `"ITRS"`, `"Slice"`, `"CanonicalSlice"`, `"SliceBoxes"`, `"Diagram"`. Pass *keys* as a `List`, [All](), or a single `String`. A single string yields a *flat* list of that field's values (one per step) instead of the list-of-associations form.

<code>[TMultiTrace]()[*expr*, *keys*, *maxSteps*]</code> caps the iteration at *maxSteps*.

## Details & Options

- Step 0 records the events that fired during *expr*'s own evaluation (so `[TMultiTrace](){}[[TCollapse]()[*t*]]` attributes the whole collapse to step 0). The per-step iteration below handles subsequent firings as the walker descends.
- Each event in `"Events"` is an `Association` with keys `"id"`, `"rule"`, `"ruleCode"`, `"family"`, `"familyCode"`, `"termA"`, `"termB"`, `"deltaLabel"`, `"consumed"`, `"produced"`. The `"family"` is one of `TERM`, `SLIDE`, `FORK`, `SPLIT`, `MERGE`, `PRUNE`, `DIST` (see `docs/multicomputation.md`).
- `"consumed"` is the wire-provenance list (event ids whose output this event's active pair read from); `"produced"` is the dual (heap locs this event wrote, last-writer-wins).
- `"SliceBoxes"` and `"Diagram"` are *expensive* (TraditionalForm rendering, DC diagram materialisation): request only what your view needs.
- `"DiagramSeeds" -> {auxTerms...}` seeds the per-step diagrams with additional roots.
- Recording is gated on the trace-enabled dylib: confirm with [TMultiTraceQ]() (default `make wl` builds it on).

## Basic Examples

A small reduction yields a handful of steps:

```wl
TReset[];
Length @ TMultiTrace[TSup[1, 2] + 3]
```
<!-- => 5 -->

A single-string key returns a flat list -- here the canonical slice per step:

```wl
TReset[];
Last @ TMultiTrace[TSup[1, 2] + 3, "CanonicalSlice"]
```
<!-- => {Term["NUM", 4], Term["NUM", 5]} -->

## Scope

Feed the steps into [TMultiwayGraph]() for the multiway view:

```wl
TReset[];
steps = TMultiTrace[TSup[1, 2] + 3];
TMultiwayGraph[steps]
```

## Properties and Relations

[TMultiwayGraph]() draws the per-slice multiway view; [TCausalGraph]() draws the event-id causal DAG. Both accept the steps record returned here. For the worked example see `docs/essays/boolean_superposition.md` and `docs/multicomputation.md`.
