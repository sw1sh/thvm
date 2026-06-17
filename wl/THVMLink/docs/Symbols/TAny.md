---
Template: Symbol
Name: TAny
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TAny
Keywords: [any, wildcard, top, equality]
SeeAlso: [TEra, TEql, TSatEUF]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TAny]</code> is the `TAG_ANY` constant -- the top of the structural-equality lattice. `Anything is equal to it`; structural rules short-circuit when an `ANY` appears on either side.

## Details & Options

- Used internally by [TEql]() and the structural-rule layer: `EQL-ANY-{L,R}` reduces to `NUM(1)` so a slot tagged `ANY` matches every term without forcing it. Compare `ERA` ([TEra]()), which prunes its partner -- `ANY` accepts it.
- An `ANY` does not get consumed by an interaction; it is a marker the reducer treats as a structural wildcard.

## Basic Examples

`TAny` is a Symbol, not a `TTerm` -- the actual atom lives in the heap layer:

```wl
TAny
```
<!-- => TAny -->

## Properties and Relations

`TAny` reads "matches anything"; `TEra[]` reads "consumes anything". They sit on opposite sides of the same axis: `ANY` is structural top, `ERA` is structural bottom. Theorem-proving paths ([TFindProof](), [TSatEUF]()) rely on the `ANY` short-circuit when a slot is intentionally unconstrained.
