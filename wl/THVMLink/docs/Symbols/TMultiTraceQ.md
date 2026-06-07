---
Template: Symbol
Name: TMultiTraceQ
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TMultiTraceQ
Keywords: [multicomputation, trace, dylib, build]
SeeAlso: [TMultiTrace, TMultiwayGraph, TCausalGraph]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TMultiTraceQ]()[]</code> returns [True]() iff the loaded `THVMLink` dylib was built with `-DTHVM_TRACE` (the default for `make wl`).

## Details & Options

- Pass `WL_TRACE=0` to `make` to opt out of the trace machinery (e.g. for benchmarking the runtime without the per-event overhead). In that case [TMultiTrace]() returns [$Failed]() and `TMultiTraceQ` is `False`.
- Cheap call -- use it as a guard at the top of a script that depends on the trace.

## Basic Examples

```wl
TMultiTraceQ[]
```
<!-- => True -->

## Properties and Relations

The trace is the foundation for [TMultiwayGraph]() (the multiway view) and [TCausalGraph]() (the causal DAG). Both require recording on; this predicate is the cheapest way to confirm.
