---
Template: Symbol
Name: TCollapse
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TCollapse
Keywords: [collapse, sup, enumerate, leaves, multicomputation, cnf]
SeeAlso: [TSup, TDup, TEra, TMultiTrace, TCnf]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TCollapse]()[*t*]</code> enumerates the SUP-tree of *t*: recursively walks `SUP` / `ERA` branches and returns a `List` of `TTerm` leaves.

<code>[TCollapse]()[*t*, *cap*]</code> caps the leaf count (default `65536`).

## Details & Options

- Drives each branch through `cnf` (collapsed normal form) -- the readback layer that lifts a nested `SUP` to the top so a `DP`-rooted head still surfaces a usable WHNF. See `src/cnf/_.c` and `docs/normal_form.md`.
- `ERA` branches contribute zero leaves; `SUP` branches recurse on each arm.
- Each leaf is a fresh `TTerm` (not the original cell) -- safe to index, compare, and feed into a follow-up reduction without aliasing the source.

## Basic Examples

A flat superposition collapses to its leaves:

```wl
TReset[];
TTermVal /@ TCollapse[TSup[{5, 7, 9}]]
```
<!-- => {5, 7, 9} -->

Arithmetic distributes through superposition before the collapse:

```wl
TReset[];
TTermVal /@ TCollapse[TSup[1, 2] + 3]
```
<!-- => {4, 5} -->

## Scope

A shared variable used twice collapses diagonally, not as a cross-product:

```wl
TReset[];
TTermVal /@ TCollapse[TLam[x, TOp2["*", x, x]][TSup[5, 7]]]
```
<!-- => {25, 49} -->

## Properties and Relations

`TCollapse` is the *observer* in the multicomputation framing -- it reads out the slice leaves of the reduction. For the per-step view of the slice evolution itself, use [TMultiTrace](); for a graph of the worlds, [TMultiwayGraph]().
