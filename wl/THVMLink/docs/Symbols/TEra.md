---
Template: Symbol
Name: TEra
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TEra
Keywords: [eraser, era, prune, multicomputation]
SeeAlso: [TAny, TCollapse, TSup, TMatNum]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TEra]()[]</code> constructs an eraser term -- the IC sink that prunes anything paired with it.

## Details & Options

- Every interaction with an `ERA` reduces to `ERA` (`APP-ERA`, `OP2-ERA`, `DUP-ERA`, ...): the term is consumed and nothing observable is produced.
- Use it to drop a branch of a superposition. [TCollapse]() walks past an `ERA` leaf without emitting it, so an `ERA` branch contributes zero leaves to the enumeration.
- An `ERA` is its own normal form; <code>[TWnf]()[*era*]</code> is a no-op.

## Basic Examples

The canonical form is just the tag:

```wl
Term[TEra[]]
```
<!-- => Term["ERA"] -->

## Scope

`ERA` prunes a branch of a superposition during collapse:

```wl
TReset[];
TTermVal /@ TCollapse[TSup[TNum[5], TEra[]]]
```
<!-- => {5} -->

## Properties and Relations

`ERA` is the *prune* side of multicomputation: SUPs add worlds, `ERA`s remove them. The PRUNE-family edges in [TCausalGraph]() and [TMultiwayGraph]() colour the `*-ERA` interactions. For an unconstrained "anything goes here" slot use [TAny]().
