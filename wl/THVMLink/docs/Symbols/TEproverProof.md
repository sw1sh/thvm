---
Template: Symbol
Name: TEproverProof
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TEproverProof
Keywords: [E prover, eprover, ATP, TPTP, SZS, external prover, CLI, paramodulation, theorem proving]
SeeAlso: [TEproverProofObject, TVampireProof, TTweeProof, TWaldmeisterProof, TSZSDerivationToProofObject, TFindProof]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TEproverProof]()[*file*]</code> runs the local E prover binary on the TPTP (Thousands of Problems for Theorem Provers) problem *file* (a path ending in `.p`) and returns a normalized result Association.

<code>[TEproverProof]()[*theory*, *thm*]</code> resolves the canonical problem file under `tools/baselines/vampire_tptp/{theory}__{thm}.p` from the (*theory*, *thm*) pair and proves it.

The result Association is keyed `"Status"` (`"Proved"`, `"TimedOut"`, or `"Failed"`), `"Strategy"` (always `"eprover-auto-schedule"`), `"Seconds"` (wall time), `"ProofLength"` (the SZS derivation step count), `"Inferences"` (the parsed SZS inference DAG), and `"RawSZS"` (the raw SZS output text).

## Details & Options

- [TEproverProof]() shells out to E with `--auto-schedule --proof-object --tstp-format`, which emits the same SZS-framed TPTP `fof` proof clauses with `inference(...)` records that Vampire's `--proof tptp` produces. The derivation therefore flows through `Wolfram`Parser`TPTPImport[..., "SZS"]` and on into [TSZSDerivationToProofObject]() with no prover-specific parsing.
- E uses inference names like `pm` (paramodulation), `rw` (rewriting), `cn` (conjunction), and `fof_nnf`; these map through `$SZSRuleToConstruct` (any unmapped name defaults to a `SubstitutionLemma` construct).
- Options:
  - `TimeConstraint` - wall-clock seconds passed to E's `--cpu-limit` (default 30).
  - `"Binary"` - an absolute path override; `Automatic` (the default) walks the Homebrew / `/usr/local` prefixes then `$PATH`, and uses the first hit.
- To lift the result into a `thvm`-shaped proof Association (or a literal `ProofObject`), use [TEproverProofObject](). See the [Method-and-preset](paclet:WolframInstitute/THVMLink/tutorial/AtpMethods) tech note for the `Method -> "EproverProcess"` dispatch path.

## Basic Examples

Run E on a canonical bench problem and read the status:

```wl
#| eval: false
Needs["WolframInstitute`THVMLink`ATP`"];
TEproverProof["AbelianGroupAxioms", "InverseOfInverse", TimeConstraint -> 10]["Status"]
```
<!-- => "Proved" -->

## Scope

Read the proof length straight off the result:

```wl
#| eval: false
Needs["WolframInstitute`THVMLink`ATP`"];
TEproverProof["AbelianGroupAxioms", "InverseOfInverse"]["ProofLength"]
```
<!-- => an integer SZS derivation step count -->

## Properties & Relations

- [TEproverProofObject]() converts this raw result into the same `thvm` proof shape the internal engine produces; the SZS lift path is shared with [TVampireProofObject]() because E and Vampire emit the same inference DAG format.
- [TVampireProof](), [TTweeProof](), and [TWaldmeisterProof]() are the sibling external-prover wrappers.

## Possible Issues

- When the E binary is absent, [TEproverProof]() returns `$Failed` and issues `TEproverProof::noeprover` (`brew install eprover`). A missing problem file issues `TEproverProof::badfile`.
