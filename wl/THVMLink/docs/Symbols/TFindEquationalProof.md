---
Template: Symbol
Name: TFindEquationalProof
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TFindEquationalProof
Keywords: [theorem proving, ATP, equational, alias]
SeeAlso: [TFindProof, FindEquationalProof, TFindEquationalPath]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TFindEquationalProof]()[*...*]</code> is equivalent to [TFindProof](); every call forwards to [TFindProof]() unchanged.

## Details & Options

- The signatures, options, and return value are exactly [TFindProof]()'s; see that page for the full surface.

## Basic Examples

The alias forwards to [TFindProof]():

```wl
TFindEquationalProof[
    x \[CircleTimes] y == y \[CircleTimes] x,
    "AbelianGroupAxioms",
    TimeConstraint -> 10]
```
<!-- => ProofObject[<|"Status" -> "Proved", ...|>] (identical to the TFindProof call) -->

## Properties & Relations

- [TFindProof]() is the current entry point. The Wolfram Language built-in [FindEquationalProof]() is the kernel's own equational prover; [TFindProof]() returns the same `ProofObject` head.
