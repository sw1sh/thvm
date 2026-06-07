---
Template: Symbol
Name: TInitialize
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TInitialize
Keywords: [snapshot, restore, context, init, persist, cross-restart]
SeeAlso: [TContextSnapshot, TContextStrip, Term, TInit]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TInitialize]()[*h*]</code> restores a `TContext` snapshot *h* into the live runtime and returns the snapshot's root as a live `TTerm` (or <code>[Missing]()["NoRoot"]</code> if the snapshot has no root).

<code>[TInitialize]()[*h*, "ZeroFill" -> True]</code> additionally accepts `Uninitialized` snapshots; tensors are allocated zero-filled.

## Details & Options

- Cross-restart capable: when `"BookCells"`, `"Defs"`, and `"AloStates"` are bundled in *h*, `TInitialize` wipes the C-side book / `DEFS` / `ALO_STATES` first, restores them, then the dyn heap. So a snapshot can survive `TFree[]` followed by [TInit]().
- Round-trips with [TContextSnapshot](): `[TInitialize](){}[[TContextSnapshot]()[*root*]]` returns a live `TTerm` whose canonical form equals *root*'s.
- Restoration is in-place: existing cells in the heap are *overwritten* up to the snapshot's `THeapPos`. Call [TReset]() or `TFree[]` first if you want a clean target.

## Basic Examples

Round-trip a small graph through a snapshot:

```wl
TReset[];
seed = TSup[1, 2] + 3;
snap = TContextSnapshot[seed];
TFree[]; TInit[];
TTermVal /@ TCollapse[TInitialize[snap]]
```
<!-- => {4, 5} -->

## Properties and Relations

[TContextSnapshot]() builds the snapshot; [TContextStrip]() removes the tensor buffers for a lightweight "shape only" version; [Term]() (the canonical form) is the in-memory walk-equivalent.
