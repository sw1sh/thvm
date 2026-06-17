---
Template: Symbol
Name: TDsu
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TDsu
Keywords: [dynamic label, dsu, sup, hvm4, lazy label]
SeeAlso: [TSup, TDdu, TFreshLabel]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TDsu]()[*label*, *a*, *b*]</code> constructs a dynamic-label SUP (HVM4 `DSU`): the *label* is itself a `TTerm` reduced strict-left at WNF time, after which `DSU` collapses to a static [TSup]() (or to <code>[TEra]()[]</code> / nested-SUP) based on what the label resolved to.

## Details & Options

- Useful for pattern compilers that need a fresh label per match instance: the label can be a [TFreshLabel]() call wrapped in a `TNum`, or a more elaborate computation that returns a `NUM`.
- When *label* resolves to <code>[TNum]()[*n*]</code>, the cell rewrites in place to <code>[TSup]()[*n*, *a*, *b*]</code> and the rest of the reduction proceeds normally.
- Label resolutions to `ERA` propagate as ERA; SUP-on-label produces a nested-SUP shape.

## Basic Examples

A dynamic-label SUP whose label resolves to 3:

```wl
TReset[];
Term @ TWnf @ TDsu[TNum[3], TNum[10], TNum[20]]
```
<!-- => Term["SUP", 3, Term["NUM", 10], Term["NUM", 20]] -->

## Properties and Relations

[TDdu]() is the DUP-side analogue. Both are an HVM4 generalisation of [TSup]() / [TDup]() that lifts the label into the term layer.
